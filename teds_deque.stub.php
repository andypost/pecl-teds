<?php

/** @generate-class-entries */
// Stub generation requires build/gen_stub.php from php 8.1 or newer.

namespace Teds;

/**
 * A double-ended queue (Typically abbreviated as Deque, pronounced "deck", like "cheque")
 * represented internally as a circular buffer.
 *
 * This has much lower memory usage than SplDoublyLinkedList or its subclasses (SplStack, SplStack),
 * and operations are significantly faster than SplDoublyLinkedList.
 *
 * See https://en.wikipedia.org/wiki/Double-ended_queue
 *
 * This supports amortized constant time pushing and popping onto the front or back of the Deque.
 *
 * Naming is based on https://www.php.net/spldoublylinkedlist
 * and on array_push/pop/unshift/shift.
 */
final class Deque implements \IteratorAggregate, Sequence, \JsonSerializable
{
    /** Construct the Deque from the values of the Traversable/array, ignoring keys */
    public function __construct(iterable $iterator = []) {}
    /**
     * Returns an iterator that accounts for calls to shift/unshift tracking the position of the start of the Deque.
     * Calls to shift/unshift will do the following:
     * - Increase/Decrease the value returned by the iterator's key()
     *   by the number of elements added/removed to/from the start of the Deque.
     *   (`$deque[$iteratorKey] === $iteratorValue` at the time the key and value are returned).
     * - Repeated calls to shift will cause valid() to return false if the iterator's
     *   position ends up before the start of the Deque at the time iteration resumes.
     * - They will not cause the remaining values to be iterated over more than once or skipped.
     */
    public function getIterator(): \InternalIterator {}
    /** Returns the number of elements in the Deque. */
    public function count(): int {}
    /** Returns true if there are 0 elements in the Deque. */
    public function isEmpty(): bool {}
    /** Removes all elements from the Deque. */
    public function clear(): void {}

    /**
     * @implementation-alias Teds\Deque::toArray
     */
    public function __serialize(): array {}
    public function __unserialize(array $data): void {}
    /** Construct the Deque from the values of the array, ignoring keys */
    public static function __set_state(array $array): Deque {}

    /** Appends value(s) to the end of the Deque, like array_push. */
    public function push(mixed ...$values): void {}
    /** Prepends value(s) to the start of the Deque, like array_unshift. */
    public function unshift(mixed ...$values): void {}
    /**
     * Pops a value from the end of the Deque.
     * @throws \UnderflowException if the Deque is empty
     */
    public function pop(): mixed {}
    /**
     * Shifts a value from the start of the Deque.
     * @throws \UnderflowException if the Deque is empty
     */
    public function shift(): mixed {}

    /**
     * Peeks at the value at the start of the Deque, throws if empty
     */
    public function first(): mixed {}
    /** @implementation-alias Teds\Deque::first */
    public function bottom(): mixed {}
    /**
     * Peeks at the value at the end of the Deque, throws if empty
     */
    public function last(): mixed {}
    /** @implementation-alias Teds\Deque::last */
    public function top(): mixed {}

    /** Returns a list of the elements from front to back. */
    public function toArray(): array {}
    /**
     * @override
     * @implementation-alias Teds\Deque::toArray
     */
    public function values(): array {}
    /* Get and set are strictly typed, unlike offsetGet/offsetSet. */
    /**
     * Returns the value at offset $offset (relative to the start of the Deque)
     * @throws \OutOfBoundsException if the value of (int)$offset is not within the bounds of this vector
     */
    public function get(int $offset): mixed {}
    /**
     * Sets the value at offset $offset (relative to the start of the Deque) to $value
     * @throws \OutOfBoundsException if the value of (int)$offset is not within the bounds of this vector
     */
    public function set(int $offset, mixed $value): void {}
    // Must be mixed for compatibility with ArrayAccess
    /**
     * Returns the value at offset (int)$offset (relative to the start of the Deque)
     * @throws \OutOfBoundsException if the value of (int)$offset is not within the bounds of this vector
     */
    public function offsetGet(mixed $offset): mixed {}
    /**
     * Returns true if `0 <= (int)$offset && (int)$offset < $this->count()
     * AND the value of offsetGet is non-null.
     */
    public function offsetExists(mixed $offset): bool {}
    /**
     * Returns true if `is_int($offset) && 0 <= $offset && $offset < $this->count().
     */
    public function containsKey(mixed $offset): bool {}
    /**
     * Sets the value at offset $offset (relative to the start of the Deque) to $value
     * @throws \OutOfBoundsException if the value of (int)$offset is not within the bounds of this vector
     */
    public function offsetSet(mixed $offset, mixed $value): void {}
    /**
     * @throws \RuntimeException unconditionally because unset and null are different things, unlike SplFixedArray
     */
    public function offsetUnset(mixed $offset): void {}


    /**
     * Returns the offset of a value that is === $value, or returns null.
     */
    public function indexOf(mixed $value): ?int {}

    /**
     * Returns true if there exists a value === $value in this Deque.
     */
    public function contains(mixed $value): bool {}

    /**
     * @implementation-alias Teds\Deque::toArray
     */
    public function jsonSerialize(): array {}

    /** @internal Returns the capacity of this Deque. This is intended for unit tests of Deque itself */
    public function capacity(): int {}
}
