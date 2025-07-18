---
description: 'Documentation for Functions for Working with IPv4 and IPv6 Addresses'
sidebar_label: 'IP Addresses'
slug: /sql-reference/functions/ip-address-functions
title: 'Functions for Working with IPv4 and IPv6 Addresses'
---

# Functions for working with IPv4 and IPv6 addresses

## IPv4NumToString {#IPv4NumToString}

Takes a UInt32 number. Interprets it as an IPv4 address in big endian. Returns a string containing the corresponding IPv4 address in the format A.B.C.d (dot-separated numbers in decimal form).

Alias: `INET_NTOA`.

## IPv4StringToNum {#IPv4StringToNum}

The reverse function of [IPv4NumToString](#IPv4NumToString). If the IPv4 address has an invalid format, it throws exception.

Alias: `INET_ATON`.

## IPv4StringToNumOrDefault(s) {#ipv4stringtonumordefaults}

Same as `IPv4StringToNum`, but if the IPv4 address has an invalid format, it returns 0.

## IPv4StringToNumOrNull(s) {#ipv4stringtonumornulls}

Same as `IPv4StringToNum`, but if the IPv4 address has an invalid format, it returns null.

## IPv4NumToStringClassC(num) {#ipv4numtostringclasscnum}

Similar to IPv4NumToString, but using xxx instead of the last octet.

Example:

```sql
SELECT
    IPv4NumToStringClassC(ClientIP) AS k,
    count() AS c
FROM test.hits
GROUP BY k
ORDER BY c DESC
LIMIT 10
```

```text
┌─k──────────────┬─────c─┐
│ 83.149.9.xxx   │ 26238 │
│ 217.118.81.xxx │ 26074 │
│ 213.87.129.xxx │ 25481 │
│ 83.149.8.xxx   │ 24984 │
│ 217.118.83.xxx │ 22797 │
│ 78.25.120.xxx  │ 22354 │
│ 213.87.131.xxx │ 21285 │
│ 78.25.121.xxx  │ 20887 │
│ 188.162.65.xxx │ 19694 │
│ 83.149.48.xxx  │ 17406 │
└────────────────┴───────┘
```

Since using 'xxx' is highly unusual, this may be changed in the future. We recommend that you do not rely on the exact format of this fragment.

### IPv6NumToString(x) {#ipv6numtostringx}

Accepts a FixedString(16) value containing the IPv6 address in binary format. Returns a string containing this address in text format.
IPv6-mapped IPv4 addresses are output in the format ::ffff:111.222.33.44.

Alias: `INET6_NTOA`.

Examples:

```sql
SELECT IPv6NumToString(toFixedString(unhex('2A0206B8000000000000000000000011'), 16)) AS addr;
```

```text
┌─addr─────────┐
│ 2a02:6b8::11 │
└──────────────┘
```

```sql
SELECT
    IPv6NumToString(ClientIP6 AS k),
    count() AS c
FROM hits_all
WHERE EventDate = today() AND substring(ClientIP6, 1, 12) != unhex('00000000000000000000FFFF')
GROUP BY k
ORDER BY c DESC
LIMIT 10
```

```text
┌─IPv6NumToString(ClientIP6)──────────────┬─────c─┐
│ 2a02:2168:aaa:bbbb::2                   │ 24695 │
│ 2a02:2698:abcd:abcd:abcd:abcd:8888:5555 │ 22408 │
│ 2a02:6b8:0:fff::ff                      │ 16389 │
│ 2a01:4f8:111:6666::2                    │ 16016 │
│ 2a02:2168:888:222::1                    │ 15896 │
│ 2a01:7e00::ffff:ffff:ffff:222           │ 14774 │
│ 2a02:8109:eee:ee:eeee:eeee:eeee:eeee    │ 14443 │
│ 2a02:810b:8888:888:8888:8888:8888:8888  │ 14345 │
│ 2a02:6b8:0:444:4444:4444:4444:4444      │ 14279 │
│ 2a01:7e00::ffff:ffff:ffff:ffff          │ 13880 │
└─────────────────────────────────────────┴───────┘
```

```sql
SELECT
    IPv6NumToString(ClientIP6 AS k),
    count() AS c
FROM hits_all
WHERE EventDate = today()
GROUP BY k
ORDER BY c DESC
LIMIT 10
```

```text
┌─IPv6NumToString(ClientIP6)─┬──────c─┐
│ ::ffff:94.26.111.111       │ 747440 │
│ ::ffff:37.143.222.4        │ 529483 │
│ ::ffff:5.166.111.99        │ 317707 │
│ ::ffff:46.38.11.77         │ 263086 │
│ ::ffff:79.105.111.111      │ 186611 │
│ ::ffff:93.92.111.88        │ 176773 │
│ ::ffff:84.53.111.33        │ 158709 │
│ ::ffff:217.118.11.22       │ 154004 │
│ ::ffff:217.118.11.33       │ 148449 │
│ ::ffff:217.118.11.44       │ 148243 │
└────────────────────────────┴────────┘
```

## IPv6StringToNum {#ipv6stringtonum}

The reverse function of [IPv6NumToString](#ipv6numtostringx). If the IPv6 address has an invalid format, it throws exception.

If the input string contains a valid IPv4 address, returns its IPv6 equivalent.
HEX can be uppercase or lowercase.

Alias: `INET6_ATON`.

**Syntax**

```sql
IPv6StringToNum(string)
```

**Argument**

- `string` — IP address. [String](../data-types/string.md).

**Returned value**

- IPv6 address in binary format. [FixedString(16)](../data-types/fixedstring.md).

**Example**

Query:

```sql
SELECT addr, cutIPv6(IPv6StringToNum(addr), 0, 0) FROM (SELECT ['notaddress', '127.0.0.1', '1111::ffff'] AS addr) ARRAY JOIN addr;
```

Result:

```text
┌─addr───────┬─cutIPv6(IPv6StringToNum(addr), 0, 0)─┐
│ notaddress │ ::                                   │
│ 127.0.0.1  │ ::ffff:127.0.0.1                     │
│ 1111::ffff │ 1111::ffff                           │
└────────────┴──────────────────────────────────────┘
```

**See Also**

- [cutIPv6](#cutipv6x-bytestocutforipv6-bytestocutforipv4).

## IPv6StringToNumOrDefault(s) {#ipv6stringtonumordefaults}

Same as `IPv6StringToNum`, but if the IPv6 address has an invalid format, it returns 0.

## IPv6StringToNumOrNull(s) {#ipv6stringtonumornulls}

Same as `IPv6StringToNum`, but if the IPv6 address has an invalid format, it returns null.

## IPv4ToIPv6(x) {#ipv4toipv6x}

Takes a `UInt32` number. Interprets it as an IPv4 address in [big endian](https://en.wikipedia.org/wiki/Endianness). Returns a `FixedString(16)` value containing the IPv6 address in binary format. Examples:

```sql
SELECT IPv6NumToString(IPv4ToIPv6(IPv4StringToNum('192.168.0.1'))) AS addr;
```

```text
┌─addr───────────────┐
│ ::ffff:192.168.0.1 │
└────────────────────┘
```

## cutIPv6(x, bytesToCutForIPv6, bytesToCutForIPv4) {#cutipv6x-bytestocutforipv6-bytestocutforipv4}

Accepts a FixedString(16) value containing the IPv6 address in binary format. Returns a string containing the address of the specified number of bytes removed in text format. For example:

```sql
WITH
    IPv6StringToNum('2001:0DB8:AC10:FE01:FEED:BABE:CAFE:F00D') AS ipv6,
    IPv4ToIPv6(IPv4StringToNum('192.168.0.1')) AS ipv4
SELECT
    cutIPv6(ipv6, 2, 0),
    cutIPv6(ipv4, 0, 2)
```

```text
┌─cutIPv6(ipv6, 2, 0)─────────────────┬─cutIPv6(ipv4, 0, 2)─┐
│ 2001:db8:ac10:fe01:feed:babe:cafe:0 │ ::ffff:192.168.0.0  │
└─────────────────────────────────────┴─────────────────────┘
```

## IPv4CIDRToRange(ipv4, Cidr), {#ipv4cidrtorangeipv4-cidr}

Accepts an IPv4 and an UInt8 value containing the [CIDR](https://en.wikipedia.org/wiki/Classless_Inter-Domain_Routing). Return a tuple with two IPv4 containing the lower range and the higher range of the subnet.

```sql
SELECT IPv4CIDRToRange(toIPv4('192.168.5.2'), 16);
```

```text
┌─IPv4CIDRToRange(toIPv4('192.168.5.2'), 16)─┐
│ ('192.168.0.0','192.168.255.255')          │
└────────────────────────────────────────────┘
```

## IPv6CIDRToRange(ipv6, Cidr), {#ipv6cidrtorangeipv6-cidr}

Accepts an IPv6 and an UInt8 value containing the CIDR. Return a tuple with two IPv6 containing the lower range and the higher range of the subnet.

```sql
SELECT IPv6CIDRToRange(toIPv6('2001:0db8:0000:85a3:0000:0000:ac1f:8001'), 32);
```

```text
┌─IPv6CIDRToRange(toIPv6('2001:0db8:0000:85a3:0000:0000:ac1f:8001'), 32)─┐
│ ('2001:db8::','2001:db8:ffff:ffff:ffff:ffff:ffff:ffff')                │
└────────────────────────────────────────────────────────────────────────┘
```

## toIPv4 {#toipv4}

Converts a string or a UInt32 form of IPv4 address to [IPv4](../data-types/ipv4.md) type.
Similar to [`IPv4StringToNum`](#IPv4StringToNum) and [IPv4NumToString](#IPv4NumToString) functions but it supports both string and unsigned integer data types as input arguments.

**Syntax**

```sql
toIPv4(x)
```

**Arguments**

- `x` — IPv4 address. [`String`](../data-types/string.md), [`UInt8/16/32`](../data-types/int-uint.md).

**Returned value**

- IPv4 address. [IPv4](../data-types/ipv4.md).

**Examples**

Query:

```sql
SELECT toIPv4('171.225.130.45');
```

Result:

```text
┌─toIPv4('171.225.130.45')─┐
│ 171.225.130.45           │
└──────────────────────────┘
```

Query:

```sql
WITH
    '171.225.130.45' AS IPv4_string
SELECT
    hex(IPv4StringToNum(IPv4_string)),
    hex(toIPv4(IPv4_string))
```

Result:

```text
┌─hex(IPv4StringToNum(IPv4_string))─┬─hex(toIPv4(IPv4_string))─┐
│ ABE1822D                          │ ABE1822D                 │
└───────────────────────────────────┴──────────────────────────┘
```


Query:

```sql
SELECT toIPv4(2130706433);
```

Result:

```text
┌─toIPv4(2130706433)─┐
│ 127.0.0.1          │
└────────────────────┘
```

## toIPv4OrDefault {#toipv4ordefault}

Same as `toIPv4`, but if the IPv4 address has an invalid format, it returns `0.0.0.0` (0 IPv4), or the provided IPv4 default.

**Syntax**

```sql
toIPv4OrDefault(string[, default])
```

**Arguments**

- `value` — IP address. [String](../data-types/string.md).
- `default` (optional) — The value to return if `string` has an invalid format. [IPv4](../data-types/ipv4.md).

**Returned value**

- `string` converted to the current IPv4 address. [String](../data-types/string.md).

**Example**

Query:

```sql
WITH
    '::ffff:127.0.0.1' AS valid_IPv6_string,
    'fe80:2030:31:24' AS invalid_IPv6_string
SELECT
    toIPv4OrDefault(valid_IPv6_string) AS valid,
    toIPv4OrDefault(invalid_IPv6_string) AS default,
    toIPv4OrDefault(invalid_IPv6_string, toIPv4('1.1.1.1')) AS provided_default;
```

Result:

```response
┌─valid───┬─default─┬─provided_default─┐
│ 0.0.0.0 │ 0.0.0.0 │ 1.1.1.1          │
└─────────┴─────────┴──────────────────┘
```

## toIPv4OrNull {#toipv4ornull}

Same as [`toIPv4`](#toipv4), but if the IPv4 address has an invalid format, it returns null.

**Syntax**

```sql
toIPv4OrNull(string)
```

**Arguments**

- `string` — IP address. [String](../data-types/string.md).

**Returned value**

- `string` converted to the current IPv4 address, or null if `string` is an invalid address. [String](../data-types/string.md).

**Example**

Query:

```sql
WITH 'fe80:2030:31:24' AS invalid_IPv6_string
SELECT toIPv4OrNull(invalid_IPv6_string);
```

Result:

```text
┌─toIPv4OrNull(invalid_IPv6_string)─┐
│ ᴺᵁᴸᴸ                              │
└───────────────────────────────────┘
```

## toIPv4OrZero {#toipv4orzero}

Same as [`toIPv4`](#toipv4), but if the IPv4 address has an invalid format, it returns `0.0.0.0`.

**Syntax**

```sql
toIPv4OrZero(string)
```

**Arguments**

- `string` — IP address. [String](../data-types/string.md).

**Returned value**

- `string` converted to the current IPv4 address, or `0.0.0.0` if `string` is an invalid address. [String](../data-types/string.md).

**Example**

Query:

```sql
WITH 'Not an IP address' AS invalid_IPv6_string
SELECT toIPv4OrZero(invalid_IPv6_string);
```

Result:

```text
┌─toIPv4OrZero(invalid_IPv6_string)─┐
│ 0.0.0.0                           │
└───────────────────────────────────┘
```

## toIPv6 {#toipv6}

Converts a string or a UInt128 form of IPv6 address to [IPv6](../data-types/ipv6.md) type. For strings, if the IPv6 address has an invalid format, returns an empty value.
Similar to [IPv6StringToNum](#ipv6stringtonum) and [IPv6NumToString](#ipv6numtostringx) functions, which convert IPv6 address to and from binary format (i.e. `FixedString(16)`).

If the input string contains a valid IPv4 address, then the IPv6 equivalent of the IPv4 address is returned.

**Syntax**

```sql
toIPv6(string)
toIPv6(UInt128)
```

**Argument**

- `x` — IP address. [`String`](../data-types/string.md) or [`UInt128`](../data-types/int-uint.md).

**Returned value**

- IP address. [IPv6](../data-types/ipv6.md).

**Examples**

Query:

```sql
WITH '2001:438:ffff::407d:1bc1' AS IPv6_string
SELECT
    hex(IPv6StringToNum(IPv6_string)),
    hex(toIPv6(IPv6_string));
```

Result:

```text
┌─hex(IPv6StringToNum(IPv6_string))─┬─hex(toIPv6(IPv6_string))─────────┐
│ 20010438FFFF000000000000407D1BC1  │ 20010438FFFF000000000000407D1BC1 │
└───────────────────────────────────┴──────────────────────────────────┘
```

Query:

```sql
SELECT toIPv6('127.0.0.1');
```

Result:

```text
┌─toIPv6('127.0.0.1')─┐
│ ::ffff:127.0.0.1    │
└─────────────────────┘
```

## toIPv6OrDefault {#toipv6ordefault}

Same as [`toIPv6`](#toipv6), but if the IPv6 address has an invalid format, it returns `::` (0 IPv6) or the provided IPv6 default.

**Syntax**

```sql
toIPv6OrDefault(string[, default])
```

**Argument**

- `string` — IP address. [String](../data-types/string.md).
- `default` (optional) — The value to return if `string` has an invalid format. [IPv6](../data-types/ipv6.md).

**Returned value**

- IPv6 address [IPv6](../data-types/ipv6.md), otherwise `::` or the provided optional default if `string` has an invalid format.

**Example**

Query:

```sql
WITH
    '127.0.0.1' AS valid_IPv4_string,
    '127.0.0.1.6' AS invalid_IPv4_string
SELECT
    toIPv6OrDefault(valid_IPv4_string) AS valid,
    toIPv6OrDefault(invalid_IPv4_string) AS default,
    toIPv6OrDefault(invalid_IPv4_string, toIPv6('1.1.1.1')) AS provided_default
```

Result:

```text
┌─valid────────────┬─default─┬─provided_default─┐
│ ::ffff:127.0.0.1 │ ::      │ ::ffff:1.1.1.1   │
└──────────────────┴─────────┴──────────────────┘
```

## toIPv6OrNull {#toipv6ornull}

Same as [`toIPv6`](#toipv6), but if the IPv6 address has an invalid format, it returns null.

**Syntax**

```sql
toIPv6OrNull(string)
```

**Argument**

- `string` — IP address. [String](../data-types/string.md).

**Returned value**

- IP address. [IPv6](../data-types/ipv6.md), or null if `string` is not a valid format.

**Example**

Query:

```sql
WITH '127.0.0.1.6' AS invalid_IPv4_string
SELECT toIPv6OrNull(invalid_IPv4_string);
```

Result:

```text
┌─toIPv6OrNull(invalid_IPv4_string)─┐
│ ᴺᵁᴸᴸ                              │
└───────────────────────────────────┘
```

## toIPv6OrZero {#toipv6orzero}

Same as [`toIPv6`](#toipv6), but if the IPv6 address has an invalid format, it returns `::`.

**Syntax**

```sql
toIPv6OrZero(string)
```

**Argument**

- `string` — IP address. [String](../data-types/string.md).

**Returned value**

- IP address. [IPv6](../data-types/ipv6.md), or `::` if `string` is not a valid format.

**Example**

Query:

```sql
WITH '127.0.0.1.6' AS invalid_IPv4_string
SELECT toIPv6OrZero(invalid_IPv4_string);
```

Result:

```text
┌─toIPv6OrZero(invalid_IPv4_string)─┐
│ ::                                │
└───────────────────────────────────┘
```

## IPv6StringToNumOrDefault(s) {#ipv6stringtonumordefaults-1}

Same as `toIPv6`, but if the IPv6 address has an invalid format, it returns 0.

## IPv6StringToNumOrNull(s) {#ipv6stringtonumornulls-1}

Same as `toIPv6`, but if the IPv6 address has an invalid format, it returns null.

## isIPv4String {#isipv4string}

Determines whether the input string is an IPv4 address or not. If `string` is IPv6 address returns `0`.

**Syntax**

```sql
isIPv4String(string)
```

**Arguments**

- `string` — IP address. [String](../data-types/string.md).

**Returned value**

- `1` if `string` is IPv4 address, `0` otherwise. [UInt8](../data-types/int-uint.md).

**Examples**

Query:

```sql
SELECT addr, isIPv4String(addr) FROM ( SELECT ['0.0.0.0', '127.0.0.1', '::ffff:127.0.0.1'] AS addr ) ARRAY JOIN addr;
```

Result:

```text
┌─addr─────────────┬─isIPv4String(addr)─┐
│ 0.0.0.0          │                  1 │
│ 127.0.0.1        │                  1 │
│ ::ffff:127.0.0.1 │                  0 │
└──────────────────┴────────────────────┘
```

## isIPv6String {#isipv6string}

Determines whether the input string is an IPv6 address or not. If `string` is IPv4 address returns `0`.

**Syntax**

```sql
isIPv6String(string)
```

**Arguments**

- `string` — IP address. [String](../data-types/string.md).

**Returned value**

- `1` if `string` is IPv6 address, `0` otherwise. [UInt8](../data-types/int-uint.md).

**Examples**

Query:

```sql
SELECT addr, isIPv6String(addr) FROM ( SELECT ['::', '1111::ffff', '::ffff:127.0.0.1', '127.0.0.1'] AS addr ) ARRAY JOIN addr;
```

Result:

```text
┌─addr─────────────┬─isIPv6String(addr)─┐
│ ::               │                  1 │
│ 1111::ffff       │                  1 │
│ ::ffff:127.0.0.1 │                  1 │
│ 127.0.0.1        │                  0 │
└──────────────────┴────────────────────┘
```

## isIPAddressInRange {#isipaddressinrange}

Determines if an IP address is contained in a network represented in the [CIDR](https://en.wikipedia.org/wiki/Classless_Inter-Domain_Routing) notation. Returns `1` if true, or `0` otherwise.

**Syntax**

```sql
isIPAddressInRange(address, prefix)
```

This function accepts both IPv4 and IPv6 addresses (and networks) represented as strings. It returns `0` if the IP version of the address and the CIDR don't match.

**Arguments**

- `address` — An IPv4 or IPv6 address. [String](../data-types/string.md), [IPv4](../data-types/ipv4.md), [IPv6](../data-types/ipv6.md), `Nullable(String)`, `Nullable(IPv4)` and `Nullable(IPv6)`.
- `prefix` — An IPv4 or IPv6 network prefix in CIDR. [String](../data-types/string.md).

**Returned value**

- `1` or `0`. [UInt8](../data-types/int-uint.md).

**Example**

Query:

```sql
SELECT isIPAddressInRange('127.0.0.1', '127.0.0.0/8');
```

Result:

```text
┌─isIPAddressInRange('127.0.0.1', '127.0.0.0/8')─┐
│                                              1 │
└────────────────────────────────────────────────┘
```

Query:

```sql
SELECT isIPAddressInRange('127.0.0.1', 'ffff::/16');
```

Result:

```text
┌─isIPAddressInRange('127.0.0.1', 'ffff::/16')─┐
│                                            0 │
└──────────────────────────────────────────────┘
```

Query:

```sql
SELECT isIPAddressInRange('::ffff:192.168.0.1', '::ffff:192.168.0.4/128');
```

Result:

```text
┌─isIPAddressInRange('::ffff:192.168.0.1', '::ffff:192.168.0.4/128')─┐
│                                                                  0 │
└────────────────────────────────────────────────────────────────────┘
```

<!-- 
The inner content of the tags below are replaced at doc framework build time with 
docs generated from system.functions. Please do not modify or remove the tags.
See: https://github.com/ClickHouse/clickhouse-docs/blob/main/contribute/autogenerated-documentation-from-source.md
-->

<!--AUTOGENERATED_START-->
<!--AUTOGENERATED_END-->
