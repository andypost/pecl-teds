--TEST--
Teds\BitSet unserialize error handling
--FILE--
<?php

call_user_func(function () {
    $ser = 'O:11:"Teds\BitSet":2:{i:0;s:5:"first";s:5:"unexp";s:6:"second";}';
    try {
        var_dump(unserialize($ser));
    } catch (Throwable $e) {
        printf("Caught %s: %s\n", $e::class, $e->getMessage());
    }
});
?>
--EXPECT--
Caught RuntimeException: Teds\BitSet unexpected __unserialize data: expected exactly 0 or 1 value