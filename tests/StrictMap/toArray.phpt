--TEST--
Teds\StrictMap toArray()
--FILE--
<?php
call_user_func(function () {
    $it = new Teds\StrictMap(['first' => 'x', 'second' => new stdClass()]);
    var_dump($it->toArray());
});
call_user_func(function () {
    $it = new Teds\StrictMap();
    $it[[]] = 123;
    try {
        var_dump($it->toArray());
    } catch (TypeError $e) {
        echo "Caught: {$e->getMessage()}\n";
    }
});

?>
--EXPECT--
array(2) {
  ["first"]=>
  string(1) "x"
  ["second"]=>
  object(stdClass)#3 (0) {
  }
}
Caught: Illegal offset type