<?php
/** @generate-class-entries */
// Stub generation requires build/gen_stubs.php from php 8.1 or newer.

// NOTE: Due to a limitation of gen_stubs.php (at)param is used instead

namespace Teds;

/**
 * Determines whether any element of the iterable satisfies the predicate.
 *
 * If the value returned by the callback is truthy
 * (e.g. true, non-zero number, non-empty array, truthy object, etc.),
 * this is treated as satisfying the predicate.
 *
 * (at)param null|callable(mixed):mixed $callback
 */
function any(iterable $iterable, ?callable $callback = null): bool {}

/**
 * Determines whether all elements of the iterable satisfy the predicate.
 *
 * If the value returned by the callback is truthy
 * (e.g. true, non-zero number, non-empty array, truthy object, etc.),
 * this is treated as satisfying the predicate.
 *
 * (at)param null|callable(mixed):mixed $callback
 */
function all(iterable $iterable, ?callable $callback = null): bool {}

/**
 * Determines whether no element of the iterable satisfies the predicate.
 *
 * If the value returned by the callback is truthy
 * (e.g. true, non-zero number, non-empty array, truthy object, etc.),
 * this is treated as satisfying the predicate.
 *
 * Equivalent to `!any($iterable, $callback)`
 *
 * (at)param null|callable(mixed):mixed $callback
 */
function none(iterable $iterable, ?callable $callback = null): bool {}

/**
 * Returns true if this iterable includes a value identical to $value (===).
 * Similar to in_array($value, $iterable, true) but also works on Traversables
 */
function includes_value(iterable $iterable, mixed $value): bool {}

/**
 * Returns the result of repeatedly applying $initial = $callback($initial, $value) for each value in $iterable
 * (at)param callable(mixed $initial, mixed $value):mixed $callback
 */
function fold(iterable $iterable, callable $callback, mixed $initial): mixed {}

/**
 * Returns the first value for which $callback($value) is truthy.
 * On failure, returns $default.
 */
function find(iterable $iterable, callable $callback, mixed $default = null): mixed {}

/**
 * Returns the first value of the array without affecting the internal array pointer.
 * If the array is empty, returns null.
 *
 * Counterpart to https://php.net/array_key_first
 */
function array_value_first(array $array): mixed { }

/**
 * Returns the last value of the array without affecting the internal array pointer.
 * If the array is empty, returns null.
 *
 * Counterpart to https://php.net/array_key_last
 */
function array_value_last(array $array): mixed { }

/**
 * Stable comparison of values.
 * Like strcmp, this returns a negative value for less than, and positive for greater than, and 0 for equality.
 * This exists because php's `<` operator is not stable. `'10' < '0a' < '1b' < '9' < '10'`.
 * Teds\stable_compare fixes that by strictly ordering:
 *
 * - `null < false < true < int,float < string < array < object < resource`.
 * - objects and resources are compared by id.
 * - arrays are compared recursively. Smaller arrays are less than larger arrays.
 * - int/float are compared numerically. If an int is equal to a float, then the int is first.
 * - strings are compared with strcmp.
 */
function stable_compare(mixed $v1, mixed $v2): int { }
