const util = require('util')

var bdb = require("..");

var dbenv = new bdb.DbEnv();
console.log("open env", dbenv.open("db"));
var db = new bdb.Db(dbenv);

var assert = require('assert');

var rand = () => (Math.random() * 1e6).toFixed(0);
var getFilename = () => `foo-${rand()}.db`
var filename = getFilename();

var openRes = db.open(filename);
console.log("opened", filename, "ret=", openRes);
console.log("truncated db", filename);
  
function test_put_get_del_trunc() {
  console.log("-- test_put_get_del_trunc");
  
  db.truncate();

  for (var i = 0; i < 100; ++i) {
    var key = i.toFixed(0);
    var val = (Math.random() * 1e6).toFixed(0);

    db.put(key, val)
    console.log("put", key);
    var str = db.get(key)
    console.log("get", key, "=>", str.toString());
    assert(str.toString() === val);
    console.log("del", key);
    db.del(key);
    var str2 = db.get(key);
    console.log("get", key, "=>", str2.toString());
    assert(str2.toString() === "");
  }
}

function test_json() {
  console.log("-- test_json");
  var opts = { json: true };
  var put_data = { test: "json1", n: 1 };
  db.put("json1", put_data, opts)
  console.log("put json1", put_data);
  var data = db.get("json1", opts);
  console.log("get json1", data);
  assert(typeof data == 'object');
  assert(data.test == put_data.test);
  assert(data.n == put_data.n);
  console.log("del json1");
  db.del("json1");
}

function test_encoding() {
  console.log("-- test_encoding");
  var opts = { encoding: "hex" };
  var enc_str = "4f4ca1";
  db.put("hex", enc_str, opts)
  console.log("put hex", enc_str);
  var out = db.get("hex", opts);
  console.log("get hex", out);
  assert(enc_str == out);
  console.log("del hex");
  db.del("hex");
}

function test_transactions() {
  console.log("-- test_transactions");

  var txn = new bdb.DbTxn(dbenv);
  var opts = { txn };

  console.log("created", txn.constructor.name);

  console.log("putting elements 1, 2, 3 in txn");
  db.put("1", "one", opts);
  db.put("2", "two", opts);
  db.put("3", "three", opts);

  var retCommit = txn.commit();
  console.log("commited transaction", retCommit);
  
  var out1 = db.get("1");
  var out2 = db.get("2");
  var out3 = db.get("3");
  console.log("get 1", out1);
  console.log("get 2", out2);
  console.log("get 3", out3);
  assert("one" == out1);
  assert("two" == out2);
  assert("three" == out3);

  var txn2 = new bdb.DbTxn(dbenv);
  var opts2 = { txn: txn2 };
  console.log("created", txn2);

  console.log("putting elements 4, 5, 6 in txn");
  db.put("4", "four", opts2);
  db.put("5", "five", opts2);
  db.put("6", "six", opts2);

  var retAbort = txn2.abort();
  console.log("aborted transaction", retAbort);

  var out4 = db.get("4");
  var out5 = db.get("5");
  var out6 = db.get("6");
  console.log("get 4", out4);
  console.log("get 5", out5);
  console.log("get 6", out6);
  assert("" == out4);
  assert("" == out5);
  assert("" == out6);

  console.log("del 1, 2, 3");
  db.del("1");
  db.del("2");
  db.del("3");
}

function test_cursors() {
  console.log("-- test_cursors");
  console.log("putting elements 1, 2, 3, 4, 5, 6");
  db.put("1", "one");
  db.put("2", "two");
  db.put("3", "three");
  db.put("4", "four");

  // var cursor = new bdb.DbCursor(db);
  var txn = new bdb.DbTxn(dbenv);
  var cursor = new bdb.DbCursor(db, txn);
  console.log("opened cursor");

  var res;
  res = cursor.next();
  console.log(res);
  assert(res.key == "1");
  assert(res.value.toString() == "one");
  
  res = cursor.last();
  console.log(res);
  assert(res.key == "4");
  assert(res.value.toString() == "four");
  
  res = cursor.prev();
  console.log(res);
  assert(res.key == "3");
  assert(res.value.toString() == "three");
  
  res = cursor.set("2");
  console.log(res);
  assert(res.key == "2");
  assert(res.value.toString() == "two");
  
  res = cursor.put("twotwo");
  assert(res == 0);

  res = cursor.current();
  console.log(res);
  assert(res.key == "2");
  assert(res.value.toString() == "twotwo");
  
  res = cursor.first();
  console.log(res);
  assert(res.key == "1");
  assert(res.value.toString() == "one");
  
  res = cursor.prev();
  console.log(res);
  assert(res.key == null);
  assert(res.value.toString() == "");
  
  cursor.last();
  res = cursor.next();
  console.log(res);
  assert(res.key == null);
  assert(res.value.toString() == "");

  cursor.first();
  cursor.del();
  cursor.next();
  cursor.del();
  cursor.next();
  cursor.del();
  cursor.next();
  cursor.del();

  cursor.close();
}

function test_flags() {
  console.log("-- test_flags");
  assert(Object.keys(bdb.Flags).length == 241);
  assert(Object.keys(bdb.DbTypes).length == 6);

  var db2 = new bdb.Db(dbenv);
  var filename2 = "thisdoesntexist.db";
  var openRes2 = db2.open(filename2, null, null, 0);
  console.log("open should fail on", filename2, "ret=", openRes2);
  assert(openRes2 == 2);
}

function test_hash() {
  console.log("-- test_hash");
  var db2 = new bdb.Db(dbenv);
  var filename2 = getFilename();
  var openRes2 = db2.open(filename2, null, bdb.DbTypes.DB_HASH);
  console.log("opened hash", filename2, "ret=", openRes2);
  assert(openRes2 == 0);
  db2.put("key1", "value1");
  db2.put("key2", "value2");
  assert(db2.get("key1") == "value1");
  assert(db2.get("key2") == "value2");
  db2.close();
}

function test_subdbs() {
  console.log("-- test_subdbs");
  var db2 = new bdb.Db(dbenv);
  var db3 = new bdb.Db(dbenv);
  var filename2 = getFilename();
  var openRes2 = db2.open(filename2, "hash", bdb.DbTypes.DB_HASH);
  console.log("opened hash", filename2, "ret=", openRes2);
  assert(openRes2 == 0);
  var openRes3 = db3.open(filename2, "tree", bdb.DbTypes.DB_BTREE);
  console.log("opened tree", filename2, "ret=", openRes3);
  assert(openRes3 == 0);
  db2.put("key1", "value1");
  db3.put("key1", "value1");
  assert(db2.get("key1") == "value1");
  assert(db3.get("key1") == "value1");
  db2.close();
  db3.close();
}

function test_associate() {
  console.log("-- test_associate");
  var db2 = new bdb.Db(dbenv);
  var db3 = new bdb.Db(dbenv);
  db3.setFlags(bdb.Flags.DB_DUP);
  var txn = new bdb.DbTxn(dbenv);
  var filename2 = getFilename();
  var openRes2 = db2.open(filename2, "hash", bdb.DbTypes.DB_HASH);
  console.log("opened primary hash", filename2, "ret=", openRes2);
  assert(openRes2 == 0);
  var openRes3 = db3.open(filename2, "tree", bdb.DbTypes.DB_BTREE);
  console.log("opened secondary tree", filename2, "ret=", openRes3);
  assert(openRes3 == 0);
  db2.associate(txn, db3, (k,v) => v, bdb.Flags.DB_CREATE);
  txn.commit();
  db2.put("key1", "value1");
  db2.put("key2", "value1");
  // assert(db2.get("key1") == "value1");
  // assert(db2.get("key2") == "value1");
  console.log(db3.get("value1").toString());
  db2.close();
  db3.close();
}

test_put_get_del_trunc();
test_json();
test_encoding();
test_transactions();
test_cursors();
test_flags();
test_hash();
test_subdbs();
test_associate();

var closeDb = db.close();
var closeEnv = dbenv.close();

console.log("closed", "db=", closeDb, "env=", closeEnv);
