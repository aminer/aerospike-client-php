--TEST--
Get - Check Map insert in bin

--FILE--
<?php
include dirname(__FILE__)."/../../astestframework/astest-phpt-loader.inc";
aerospike_phpt_runtest("Get", "testEmptyMapToEmptyArray");
--EXPECT--
OK
