--TEST--
Teds\Vector push/pop
--FILE--
<?php

function expect_throws(Closure $cb): void {
    try {
        $cb();
        echo "Unexpectedly didn't throw\n";
    } catch (Throwable $e) {
        printf("Caught %s: %s\n", $e::class, $e->getMessage());
    }
}

echo "Test empty vector\n";
$it = new Teds\Vector([]);
expect_throws(fn() => $it->pop());
expect_throws(fn() => $it->pop());
$it->push(strtoupper('test'));
$it->push(['literal']);
$it->push(new stdClass());
echo json_encode($it), "\n";
printf("count=%d\n", count($it));
var_dump($it->pop());
var_dump($it->pop());
echo "After popping 2 elements: ", json_encode($it->toArray()), "\n";
var_dump($it->pop());
echo json_encode($it), "\n";
printf("count=%d\n", count($it));

?>
--EXPECT--
Test empty vector
Caught RuntimeException: Cannot pop from empty vector
Caught RuntimeException: Cannot pop from empty vector
["TEST",["literal"],{}]
count=3
object(stdClass)#2 (0) {
}
array(1) {
  [0]=>
  string(7) "literal"
}
After popping 2 elements: ["TEST"]
string(4) "TEST"
[]
count=0