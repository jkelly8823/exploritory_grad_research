SplFileObject::fputcsv(): Checking data after calling the function
--FILE--
<?php
$fo = new SplFileObject('SplFileObject_fputcsv.csv', 'w');

$data = array(1, 2, 'foo', 'haha', array(4, 5, 6), 1.3, null);

$fo->fputcsv($data);

var_dump($data);
?>
--CLEAN--
<?php
unlink('SplFileObject::fputcsv.csv');
?>
--EXPECTF--
Notice: Array to string conversion in %s on line %d
array(7) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  string(3) "foo"
  [3]=>
  string(4) "haha"
  [4]=>
  array(3) {
    [0]=>
    int(4)
    [1]=>
    int(5)
    [2]=>
    int(6)
  }
  [5]=>
  float(1.3)
  [6]=>
  NULL
}
SplFileObject::fputcsv(): Checking data after calling the function
--FILE--
<?php
$fo = new SplFileObject('SplFileObject_fputcsv.csv', 'w');

$data = array(1, 2, 'foo', 'haha', array(4, 5, 6), 1.3, null);

$fo->fputcsv($data);

var_dump($data);
?>
--CLEAN--
<?php
unlink('SplFileObject::fputcsv.csv');
?>
--EXPECTF--
Notice: Array to string conversion in %s on line %d
array(7) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  string(3) "foo"
  [3]=>
  string(4) "haha"
  [4]=>
  array(3) {
    [0]=>
    int(4)
    [1]=>
    int(5)
    [2]=>
    int(6)
  }
  [5]=>
  float(1.3)
  [6]=>
  NULL
}