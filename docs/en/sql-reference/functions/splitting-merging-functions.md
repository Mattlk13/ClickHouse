---
description: 'Documentation for Functions for Splitting Strings'
sidebar_label: 'String splitting'
slug: /sql-reference/functions/splitting-merging-functions
title: 'Functions for Splitting Strings'
---

import DeprecatedBadge from '@theme/badges/DeprecatedBadge';

# Functions for splitting strings

## splitByChar {#splitbychar}

Splits a string into substrings separated by a specified character. Uses a constant string `separator` which consists of exactly one character.
Returns an array of selected substrings. Empty substrings may be selected if the separator occurs at the beginning or end of the string, or if there are multiple consecutive separators.

**Syntax**

```sql
splitByChar(separator, s[, max_substrings]))
```

**Arguments**

- `separator` — The separator must be a single-byte character. [String](../data-types/string.md).
- `s` — The string to split. [String](../data-types/string.md).
- `max_substrings` — An optional `Int64` defaulting to 0. If `max_substrings` > 0, the returned array will contain at most `max_substrings` substrings, otherwise the function will return as many substrings as possible.

**Returned value(s)**

- An array of selected substrings. [Array](../data-types/array.md)([String](../data-types/string.md)).

 Empty substrings may be selected when:

- A separator occurs at the beginning or end of the string;
- There are multiple consecutive separators;
- The original string `s` is empty.

:::note
The behavior of parameter `max_substrings` changed starting with ClickHouse v22.11. In versions older than that, `max_substrings > 0` meant that `max_substring`-many splits were performed and that the remainder of the string was returned as the final element of the list.
For example,
- in v22.10: `SELECT splitByChar('=', 'a=b=c=d', 2);` returned `['a','b','c=d']`
- in v22.11: `SELECT splitByChar('=', 'a=b=c=d', 2);` returned `['a','b']`

A behavior similar to ClickHouse pre-v22.11 can be achieved by setting
[splitby_max_substrings_includes_remaining_string](../../operations/settings/settings.md#splitby_max_substrings_includes_remaining_string)
`SELECT splitByChar('=', 'a=b=c=d', 2) SETTINGS splitby_max_substrings_includes_remaining_string = 1 -- ['a', 'b=c=d']`
:::

**Example**

```sql
SELECT splitByChar(',', '1,2,3,abcde');
```

Result:

```text
┌─splitByChar(',', '1,2,3,abcde')─┐
│ ['1','2','3','abcde']           │
└─────────────────────────────────┘
```

## splitByString {#splitbystring}

Splits a string into substrings separated by a string. It uses a constant string `separator` of multiple characters as the separator. If the string `separator` is empty, it will split the string `s` into an array of single characters.

**Syntax**

```sql
splitByString(separator, s[, max_substrings]))
```

**Arguments**

- `separator` — The separator. [String](../data-types/string.md).
- `s` — The string to split. [String](../data-types/string.md).
- `max_substrings` — An optional `Int64` defaulting to 0. When `max_substrings` > 0, the returned substrings will be no more than `max_substrings`, otherwise the function will return as many substrings as possible.

**Returned value(s)**

- An array of selected substrings. [Array](../data-types/array.md)([String](../data-types/string.md)).

Empty substrings may be selected when:

- A non-empty separator occurs at the beginning or end of the string;
- There are multiple consecutive non-empty separators;
- The original string `s` is empty while the separator is not empty.

:::note
Setting [splitby_max_substrings_includes_remaining_string](../../operations/settings/settings.md#splitby_max_substrings_includes_remaining_string) (default: 0) controls if the remaining string is included in the last element of the result array when argument `max_substrings` > 0.
:::

**Example**

```sql
SELECT splitByString(', ', '1, 2 3, 4,5, abcde');
```

Result:

```text
┌─splitByString(', ', '1, 2 3, 4,5, abcde')─┐
│ ['1','2 3','4,5','abcde']                 │
└───────────────────────────────────────────┘
```

```sql
SELECT splitByString('', 'abcde');
```

Result:

```text
┌─splitByString('', 'abcde')─┐
│ ['a','b','c','d','e']      │
└────────────────────────────┘
```

## splitByRegexp {#splitbyregexp}

Splits a string into substrings separated by a regular expression. It uses a regular expression string `regexp` as the separator. If the `regexp` is empty, it will split the string `s` into an array of single characters. If no match is found for this regular expression, the string `s` won't be split.

**Syntax**

```sql
splitByRegexp(regexp, s[, max_substrings]))
```

**Arguments**

- `regexp` — Regular expression. Constant. [String](../data-types/string.md) or [FixedString](../data-types/fixedstring.md).
- `s` — The string to split. [String](../data-types/string.md).
- `max_substrings` — An optional `Int64` defaulting to 0. When `max_substrings` > 0, the returned substrings will be no more than `max_substrings`, otherwise the function will return as many substrings as possible.


**Returned value(s)**

- An array of selected substrings. [Array](../data-types/array.md)([String](../data-types/string.md)).


Empty substrings may be selected when:

- A non-empty regular expression match occurs at the beginning or end of the string;
- There are multiple consecutive non-empty regular expression matches;
- The original string `s` is empty while the regular expression is not empty.

:::note
Setting [splitby_max_substrings_includes_remaining_string](../../operations/settings/settings.md#splitby_max_substrings_includes_remaining_string) (default: 0) controls if the remaining string is included in the last element of the result array when argument `max_substrings` > 0.
:::

**Example**

```sql
SELECT splitByRegexp('\\d+', 'a12bc23de345f');
```

Result:

```text
┌─splitByRegexp('\\d+', 'a12bc23de345f')─┐
│ ['a','bc','de','f']                    │
└────────────────────────────────────────┘
```

```sql
SELECT splitByRegexp('', 'abcde');
```

Result:

```text
┌─splitByRegexp('', 'abcde')─┐
│ ['a','b','c','d','e']      │
└────────────────────────────┘
```

## splitByWhitespace {#splitbywhitespace}

Splits a string into substrings separated by whitespace characters. 
Returns an array of selected substrings.

**Syntax**

```sql
splitByWhitespace(s[, max_substrings]))
```

**Arguments**

- `s` — The string to split. [String](../data-types/string.md).
- `max_substrings` — An optional `Int64` defaulting to 0. When `max_substrings` > 0, the returned substrings will be no more than `max_substrings`, otherwise the function will return as many substrings as possible.


**Returned value(s)**

- An array of selected substrings. [Array](../data-types/array.md)([String](../data-types/string.md)).
 
:::note
Setting [splitby_max_substrings_includes_remaining_string](../../operations/settings/settings.md#splitby_max_substrings_includes_remaining_string) (default: 0) controls if the remaining string is included in the last element of the result array when argument `max_substrings` > 0.
:::

**Example**

```sql
SELECT splitByWhitespace('  1!  a,  b.  ');
```

Result:

```text
┌─splitByWhitespace('  1!  a,  b.  ')─┐
│ ['1!','a,','b.']                    │
└─────────────────────────────────────┘
```

## splitByNonAlpha {#splitbynonalpha}

Splits a string into substrings separated by whitespace and punctuation characters. 
Returns an array of selected substrings.

**Syntax**

```sql
splitByNonAlpha(s[, max_substrings]))
```

**Arguments**

- `s` — The string to split. [String](../data-types/string.md).
- `max_substrings` — An optional `Int64` defaulting to 0. When `max_substrings` > 0, the returned substrings will be no more than `max_substrings`, otherwise the function will return as many substrings as possible.


**Returned value(s)**

- An array of selected substrings. [Array](../data-types/array.md)([String](../data-types/string.md)).

:::note
Setting [splitby_max_substrings_includes_remaining_string](../../operations/settings/settings.md#splitby_max_substrings_includes_remaining_string) (default: 0) controls if the remaining string is included in the last element of the result array when argument `max_substrings` > 0.
:::

**Example**

```sql
SELECT splitByNonAlpha('  1!  a,  b.  ');
```

```text
┌─splitByNonAlpha('  1!  a,  b.  ')─┐
│ ['1','a','b']                     │
└───────────────────────────────────┘
```

## arrayStringConcat {#arraystringconcat}

Concatenates string representations of values listed in the array with the separator. `separator` is an optional parameter: a constant string, set to an empty string by default.
Returns the string.

**Syntax**

```sql
arrayStringConcat(arr\[, separator\])
```

**Example**

```sql
SELECT arrayStringConcat(['12/05/2021', '12:50:00'], ' ') AS DateString;
```

Result:

```text
┌─DateString──────────┐
│ 12/05/2021 12:50:00 │
└─────────────────────┘
```

## alphaTokens {#alphatokens}

Selects substrings of consecutive bytes from the ranges a-z and A-Z.Returns an array of substrings.

**Syntax**

```sql
alphaTokens(s[, max_substrings]))
```

Alias: `splitByAlpha`

**Arguments**

- `s` — The string to split. [String](../data-types/string.md).
- `max_substrings` — An optional `Int64` defaulting to 0. When `max_substrings` > 0, the returned substrings will be no more than `max_substrings`, otherwise the function will return as many substrings as possible.

**Returned value(s)**

- An array of selected substrings. [Array](../data-types/array.md)([String](../data-types/string.md)).

:::note
Setting [splitby_max_substrings_includes_remaining_string](../../operations/settings/settings.md#splitby_max_substrings_includes_remaining_string) (default: 0) controls if the remaining string is included in the last element of the result array when argument `max_substrings` > 0.
:::

**Example**

```sql
SELECT alphaTokens('abca1abc');
```

```text
┌─alphaTokens('abca1abc')─┐
│ ['abca','abc']          │
└─────────────────────────┘
```

## extractAllGroups {#extractallgroups}

Extracts all groups from non-overlapping substrings matched by a regular expression.

**Syntax**

```sql
extractAllGroups(text, regexp)
```

**Arguments**

- `text` — [String](../data-types/string.md) or [FixedString](../data-types/fixedstring.md).
- `regexp` — Regular expression. Constant. [String](../data-types/string.md) or [FixedString](../data-types/fixedstring.md).

**Returned values**

- If the function finds at least one matching group, it returns `Array(Array(String))` column, clustered by group_id (1 to N, where N is number of capturing groups in `regexp`). If there is no matching group, it returns an empty array. [Array](../data-types/array.md).

**Example**

```sql
SELECT extractAllGroups('abc=123, 8="hkl"', '("[^"]+"|\\w+)=("[^"]+"|\\w+)');
```

Result:

```text
┌─extractAllGroups('abc=123, 8="hkl"', '("[^"]+"|\\w+)=("[^"]+"|\\w+)')─┐
│ [['abc','123'],['8','"hkl"']]                                         │
└───────────────────────────────────────────────────────────────────────┘
```

## ngrams {#ngrams}

Splits a UTF-8 string into n-grams of `ngramsize` symbols.

**Syntax**

```sql
ngrams(string, ngramsize)
```

**Arguments**

- `string` — String. [String](../data-types/string.md) or [FixedString](../data-types/fixedstring.md).
- `ngramsize` — The size of an n-gram. [UInt](../data-types/int-uint.md).

**Returned values**

- Array with n-grams. [Array](../data-types/array.md)([String](../data-types/string.md)).

**Example**

```sql
SELECT ngrams('ClickHouse', 3);
```

Result:

```text
┌─ngrams('ClickHouse', 3)───────────────────────────┐
│ ['Cli','lic','ick','ckH','kHo','Hou','ous','use'] │
└───────────────────────────────────────────────────┘
```

## tokens {#tokens}

Splits a string into tokens using the given tokenizer.
The default tokenizer uses non-alphanumeric ASCII characters as separators.

**Arguments**

- `value` — The input string. [String](../data-types/string.md) or [FixedString](../data-types/fixedstring.md).
- `tokenizer` — The tokenizer to use. Valid arguments are `default`, `ngram`, `split`, and `no_op`. Optional, if not set explicitly, defaults to `default`. [const String](../data-types/string.md)
- `ngrams` — Only relevant if argument `tokenizer` is `ngram`: An optional parameter which defines the length of the ngrams. If not set explicitly, defaults to `3`. [UInt8](../data-types/int-uint.md).
- `separators` — Only relevant if argument `tokenizer` is `split`: An optional parameter which defines the separator strings. If not set explicitly, defaults to `[' ']`. [Array(String)](../data-types/array.md).

:::note
In case of the `split` tokenizer: if the tokens do not form a [prefix code](https://en.wikipedia.org/wiki/Prefix_code), you likely want that the matching prefers longer separators first.
To do so, pass the separators in order of descending length.
For example, with separators = `['%21', '%']` string `%21abc` would be tokenized as `['abc']`, whereas separators = `['%', '%21']` would tokenize to `['21ac']` (which is likely not what you wanted).
:::

**Returned value**

- The resulting array of tokens from input string. [Array](../data-types/array.md).

**Example**

Using the default settings:

```sql
SELECT tokens('test1,;\\ test2,;\\ test3,;\\   test4') AS tokens;
```

Result:

```text
┌─tokens────────────────────────────┐
│ ['test1','test2','test3','test4'] │
└───────────────────────────────────┘
```

Using the ngram tokenizer with ngram length 3:

```sql
SELECT tokens('abc def', 'ngram', 3) AS tokens;
```

Result:

```text
┌─tokens──────────────────────────┐
│ ['abc','bc ','c d',' de','def'] │
└─────────────────────────────────┘
```

<!-- 
The inner content of the tags below are replaced at doc framework build time with 
docs generated from system.functions. Please do not modify or remove the tags.
See: https://github.com/ClickHouse/clickhouse-docs/blob/main/contribute/autogenerated-documentation-from-source.md
-->

<!--AUTOGENERATED_START-->
<!--AUTOGENERATED_END-->
