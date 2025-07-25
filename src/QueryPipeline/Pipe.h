#pragma once

#include <Core/Block_fwd.h>
#include <Processors/IProcessor.h>

#include <functional>

namespace DB
{

class Chain;
class EnabledQuota;
struct StreamLocalLimits;

class Pipe;
using Pipes = std::vector<Pipe>;

class ReadProgressCallback;

using OutputPortRawPtrs = std::vector<OutputPort *>;

/// Pipe is a set of processors which represents the part of pipeline.
/// Pipe contains a list of output ports, with specified port for totals and specified port for extremes.
/// All output ports have same header.
/// All other ports are connected, all connections are inside processors set.
class Pipe
{
public:
    /// Default constructor creates empty pipe. Generally, you cannot do anything with it except to check it is empty().
    /// You cannot get empty pipe in any other way. All transforms check that result pipe is not empty.
    Pipe();
    /// Create from source. Source must have no input ports and single output.
    explicit Pipe(ProcessorPtr source);
    /// Create from source with specified totals end extremes (may be nullptr). Ports should be owned by source.
    explicit Pipe(ProcessorPtr source, OutputPort * output, OutputPort * totals, OutputPort * extremes);
    /// Create from processors. Use all not-connected output ports as output_ports. Check invariants.
    explicit Pipe(std::shared_ptr<Processors> processors_);

    Pipe(const Pipe & other) = delete;
    Pipe(Pipe && other) = default;
    Pipe & operator=(const Pipe & other) = delete;
    Pipe & operator=(Pipe && other) = default;

    const Block & getHeader() const { return *header; }
    const SharedHeader & getSharedHeader() const { return header; }
    bool empty() const { return processors->empty(); }
    size_t numOutputPorts() const { return output_ports.size(); }
    size_t maxParallelStreams() const { return max_parallel_streams; }
    OutputPort * getOutputPort(size_t pos) const { return output_ports[pos]; }
    OutputPort * getTotalsPort() const { return totals_port; }
    OutputPort * getExtremesPort() const { return extremes_port; }

    /// Add processor to list, add it output ports to output_ports.
    /// Processor shouldn't have input ports, output ports shouldn't be connected.
    /// Output headers should have same structure and be compatible with current header (if not empty()).
    void addSource(ProcessorPtr source);

    /// Add totals and extremes.
    void addTotalsSource(ProcessorPtr source);
    void addExtremesSource(ProcessorPtr source);

    /// Drop totals and extremes (create NullSink for them).
    void dropTotals();
    void dropExtremes();

    /// Add processor to list. It should have size() input ports with compatible header.
    /// Output ports should have same headers.
    /// If totals or extremes are not empty, transform shouldn't change header.
    void addTransform(ProcessorPtr transform);
    void addTransform(ProcessorPtr transform, OutputPort * totals, OutputPort * extremes);
    void addTransform(ProcessorPtr transform, InputPort * totals, InputPort * extremes);

    void addTransform(
        ProcessorPtr transform,
        InputPort * totals_in, InputPort * extremes_in,
        OutputPort * totals_out, OutputPort * extremes_out);

    enum class StreamType : uint8_t
    {
        Main = 0, /// Stream for query data. There may be several streams of this type.
        Totals,  /// Stream for totals. No more than one.
        Extremes, /// Stream for extremes. No more than one.
    };

    using ProcessorGetterSharedHeader = std::function<ProcessorPtr(const SharedHeader & header)>;
    using ProcessorGetterSharedHeaderWithStreamKind = std::function<ProcessorPtr(const SharedHeader & header, StreamType stream_type)>;

    /// Add transform with single input and single output for each port.
    void addSimpleTransform(const ProcessorGetterSharedHeader & getter);
    void addSimpleTransform(const ProcessorGetterSharedHeaderWithStreamKind & getter);

    /// Add chain to every output port.
    void addChains(std::vector<Chain> chains);

    /// Changes the number of output ports if needed. Adds (Strict)ResizeProcessor.
    void resize(size_t num_streams, bool strict = false, UInt64 min_outstreams_per_resize_after_split = 0);

    using Transformer = std::function<Processors(OutputPortRawPtrs ports)>;

    /// Transform Pipe in general way.
    void transform(const Transformer & transformer, bool check_ports = true);

    /// Unite several pipes together. They should have same header.
    static Pipe unitePipes(Pipes pipes);

    /// Get processors from Pipe. Use it with caution, it is easy to lose totals and extremes ports.
    static Processors detachProcessors(Pipe pipe) { return *std::move(pipe.processors); }
    /// Get processors from Pipe without destroying pipe (used for EXPLAIN to keep QueryPlan).
    const Processors & getProcessors() const { return *processors; }

    std::shared_ptr<Processors> getProcessorsPtr() { return processors; }

private:
    /// Header is common for all output below.
    SharedHeader header;
    std::shared_ptr<Processors> processors;

    /// Output ports. Totals and extremes are allowed to be empty.
    OutputPortRawPtrs output_ports;
    OutputPort * totals_port = nullptr;
    OutputPort * extremes_port = nullptr;

    /// It is the max number of processors which can be executed in parallel for each step.
    /// Usually, it's the same as the number of output ports.
    size_t max_parallel_streams = 0;

    /// If is set, all newly created processors will be added to this too.
    /// It is needed for debug. See QueryPipelineProcessorsCollector.
    Processors * collected_processors = nullptr;

    /// This methods are for QueryPipeline. It is allowed to complete graph only there.
    /// So, we may be sure that Pipe always has output port if not empty.
    bool isCompleted() const { return !empty() && output_ports.empty(); }
    static Pipe unitePipes(Pipes pipes, Processors * collected_processors, bool allow_empty_header);
    void setSinks(const Pipe::ProcessorGetterSharedHeaderWithStreamKind & getter);
    void addSplitResizeTransform(size_t num_streams, size_t min_outstreams_per_resize_after_split, bool strict);

    friend class QueryPipelineBuilder;
    friend class QueryPipeline;
};

}
