#include <Processors/Transforms/SquashingTransform.h>
#include <Interpreters/Squashing.h>

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
}

SquashingTransform::SquashingTransform(
    const Block & header, size_t min_block_size_rows, size_t min_block_size_bytes)
    : ExceptionKeepingTransform(header, header, false)
    , squashing(header, min_block_size_rows, min_block_size_bytes)
{
}

void SquashingTransform::onConsume(Chunk chunk)
{
    cur_chunk = DB::Squashing::squash(squashing.add(std::move(chunk)));
}

SquashingTransform::GenerateResult SquashingTransform::onGenerate()
{
    GenerateResult res;
    res.chunk = std::move(cur_chunk);
    res.is_done = true;
    return res;
}

void SquashingTransform::onFinish()
{
    finish_chunk = DB::Squashing::squash(squashing.flush());
}

void SquashingTransform::work()
{
    if (stage == Stage::Exception)
    {
        data.chunk.clear();
        ready_input = false;
        return;
    }

    ExceptionKeepingTransform::work();

    if (finish_chunk)
    {
        data.chunk = std::move(finish_chunk);
        ready_output = true;
    }
}

SimpleSquashingTransform::SimpleSquashingTransform(
    const Block & header, size_t min_block_size_rows, size_t min_block_size_bytes)
    : ISimpleTransform(header, header, false)
    , squashing(header, min_block_size_rows, min_block_size_bytes)
{
}

void SimpleSquashingTransform::transform(Chunk & chunk)
{
    if (!finished)
    {
        chunk = DB::Squashing::squash(squashing.add(std::move(chunk)));
    }
    else
    {
        if (chunk.hasRows())
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Chunk expected to be empty, otherwise it will be lost");

        chunk = DB::Squashing::squash(squashing.flush());
    }
}

IProcessor::Status SimpleSquashingTransform::prepare()
{
    if (!finished && input.isFinished())
    {
        if (output.isFinished())
            return Status::Finished;

        if (!output.canPush())
            return Status::PortFull;

        if (has_output)
        {
            output.pushData(std::move(output_data));
            has_output = false;
            return Status::PortFull;
        }

        finished = true;
        /// On the next call to transform() we will return all data buffered in `squashing` (if any)
        return Status::Ready;
    }
    return ISimpleTransform::prepare();
}
}
