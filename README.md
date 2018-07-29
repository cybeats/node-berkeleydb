# node-berkeleydb

Nodejs bindings for Berkeley DB 6.x

Based on the initial work [dbstore](https://github.com/leei/node-dbstore) by Lee Iverson.

## Installation

  `npm install berkeleydb`

## Usage

  `var bdb = require("berkeleydb");`

All calls are synchronous, since berkeleydb itself is synchronous.

### DB

Represents a Berkeley DB database object. Provides a simple `put`/`get`/`del` synchronous interface.

```node
var bdb = require("berkeleydb");

var db = new bdb.Db(); // create a new Db object
dbenv.open("filename.db");

var key = "foo";
var val = "bar";

// data access
db.put(key, val); // put
var out1 = db.get(key) // get
db.del(key); // del
var out2 = db.get(key); // get deleted key

assert(out1.toString() === val);
assert(out2.toString() === "");

// delete all keys
db.truncate();

db.close()
```

* `new bdb.Db([dbenv])` - Creates a new Db instance.
  - param: `[dbenv]` - `[bdb.DbEnv]` - Optional, but needed if you wish to use transactions.
  - returns `[bdb.Db]` - A new Db instance.
* `open(filename, [db])` - Opens a local db file. Will create file if it doesn't exist.
  - param: `filename` - `[String]` - The filename of the db to load, relative to the process.cwd.
  - param: `db` - `[String]` - The name of the sub-database to load.
  - returns `[number]` - 0 if successful, otherwise an error occurred.
* `close()` - Closes a database file. This is neccessary before shutdown to avoid data corruption.
  - returns `[number]` - 0 if successful, otherwise an error occurred.
* `get(key, [opts])` - Gets a value from the db.
  - param: `key` - `[String]` - The key of the value to get.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[Buffer|String|Object]` - The data stored at the key - `""` if the key doesn't exist, a `String` is returned if `opts.encoding` is set, an `Object` if `opts.json` is set to `true`, otherwise as a `Buffer`.
* `put(key, val, [opts])` - Stores or updates a value at the given key.
  - param: `key` - `[String]` - The key to store/update.
  - param: `val` - `[Buffer|String|Object]` - The value to store.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[number]` - 0 if successful, otherwise an error occurred.
* `del(key, [opts])` - Deletes a key.
  - param: `key` - `[String]` - The key to delete.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[number]` - 0 if successful, otherwise an error occurred.
* `truncate()` - Deletes all keys in the db.
  - returns `[number]` - 0 if successful, otherwise an error occurred.

#### Options object

The following options are available for the `put`/`get`/`del` methods:

* `opts.json` - `[Boolean]` - Store or retrieve value as a json object. Uses JSON.stringify/JSON.parse on value.
* `opts.encoding` - `[String]` - If specified, the buffer will be encoded/decoded as the specified format and stored/returned as a String.
* `opts.txn` - `[bdb.DbTxn]` - Apply the operation to the given transaction. This will not perform the operation until the transaction is commited.

```node
// get/put support 'json' as an option
var opts = { json: true };
var put_data = { test: "json1", n: 1 };
db.put("json", put_data, opts)
var data = db.get("json", opts);

assert(typeof data == 'object');
assert(data.test == put_data.test);
assert(data.n == put_data.n);


// get/put support 'json' as an option
var opts = { encoding: 'hex' };
var enc_str = "4f4ca1";
db.put("hex", enc_str, opts)
var out = db.get("hex", opts);

assert(enc_str == out);
```

### DB_ENV

A DbEnv enables logging, transactions and other berkeley db features, and instead of using a single file, it can store multiple files (dbs) in a directory, along with meta data.

```node
var dbenv = new bdb.DbEnv();
console.log("open env", dbenv.open("db"));

// Pass the dbenv into the db constructor
var db = new bdb.Db(dbenv);
dbenv.open("filename.db");

...

// make sure to close the db first
db.close();
dbenv.close();
```

* `new bdb.DbEnv()` - Creates a new DbEnv instance.
  - returns `[bdb.DbEnv]` - A new DbEnv instance.
* `open(dirname)` - Opens a local db env directory. The folder must exist before execution, but may be empty uninitialized.
  - param: `dirname` - `[String]` - The path of the db directory to use, relative to the process.cwd.
  - returns `[number]` - 0 if successful, otherwise an error occurred.
* `close()` - Closes a database dir. This is neccessary before shutdown to avoid data corruption, but after `Db.close()`.
  - returns `[number]` - 0 if successful, otherwise an error occurred.

### DB_TXN

Transactions provides ACID-ity to the db operations. See [Berkeley Db Transaction Documentation](http://docs.oracle.com/cd/E17076_05/html/gsg_txn/C/index.html) for full explanation.

```node
  // pass DbEnv into DbTxn constructor
  var txn = new bdb.DbTxn(dbenv);
  var txn2 = new bdb.DbTxn(dbenv);
  // pass txn into options
  var opts = { "txn": txn };
  var opts2 = { "txn": txn2 };


  // commit
  db.put("1", "one", opts);
  db.put("2", "two", opts);
  db.put("3", "three", opts);
  txn.commit();
  var out1 = db.get("1");
  var out2 = db.get("2");
  var out3 = db.get("3");

  assert("one" == out1);
  assert("two" == out2);
  assert("three" == out3);


  // abort
  db.put("4", "four", opts2);
  db.put("5", "five", opts2);
  db.put("6", "six", opts2);
  txn2.abort();
  var out4 = db.get("4");
  var out5 = db.get("5");
  var out6 = db.get("6");

  assert("" == out4);
  assert("" == out5);
  assert("" == out6);
```

* `new bdb.DbTxn([dbenv])` - Creates a new Db instance.
  - param: `[dbenv]` - `[bdb.DbEnv]` - The env to acquire a txn from.
  - returns `[bdb.DbTxn]` - A new DbTxn instance.
* `commit()` - Commits the operations associated to the transaction, with appropriate locking.
  - returns `[number]` - 0 if successful, otherwise an error occurred.
* `abort()` - Aborts the transaction.
  - returns `[number]` - 0 if successful, otherwise an error occurred.

### DBCursor

Represents a Berkeley DBCursor database object. Allows moving the cursor to a given record, fetching the current record and enumerate over all keys, forward or backward.

```js
// create the cursor, pass in the Db, and a new DbTxn if the Db is in an env
var txn = new bdb.DbTxn(dbenv);
var cursor = new bdb.DbCursor(db txn);

// put some sample data in the db
db.put("1", "one");
db.put("2", "two");
db.put("3", "three");
db.put("4", "four");

// get and move the cursor to the next element (starts at first, if not set)
var res = cursor.next();
assert(res.key == "1");
assert(res.value.toString() == "one");

// get and move the cursor to the last element
res = cursor.last();
assert(res.key == "4");
assert(res.value.toString() == "four");

// get and move the cursor to the prev element
res = cursor.prev();
assert(res.key == "3");
assert(res.value.toString() == "three");

// get and move the cursor to element with key = "2"
res = cursor.set("2");
assert(res.key == "2");
assert(res.value.toString() == "two");

// put a value at the current element
res = cursor.put("twotwo");
assert(res == 0);

// get the current element, dont move
res = cursor.current();
assert(res.key == "2");
assert(res.value.toString() == "twotwo");

// get and move the cursor to the first element
res = cursor.first();
assert(res.key == "1");
assert(res.value.toString() == "one");

// before the first element isnull
res = cursor.prev();
assert(res.key == null);
assert(res.value.toString() == "");

// after the last element is also null
res = cursor.last();
res = cursor.next();
assert(res.key == null);
assert(res.value.toString() == "");

// iterate over all elements and delete each one
cursor.first();
cursor.del();
cursor.next();
cursor.del();
cursor.next();
cursor.del();
cursor.next();
cursor.del();

// deleting removes without moving the cursor position
res = cursor.current();
assert(res.key == null);
assert(res.value.toString() == "");

cursor.close();
```

* `new bdb.DbCursor([db])` - Creates a new DbCursor instance.
  - param: `[db]` - `[bdb.Db]` - The database to create the cursor in.
  - returns `[bdb.DbCursor]` - A new DbCursor instance.
* `close()` - Closes a the cursor. No more data access is allowed.
  - returns `[number]` - 0 if successful, otherwise an error occurred.
* `current([opts])` - Gets the current cursor key and value from the db. Does not move the cursor.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[Object] {key: [String], value: [Buffer|String|Object]}` - The key and value for the current element.
* `next([opts])` - Gets the next cursor key and value from the db. Moves the cursor to the next element.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[Object] {key: [String], value: [Buffer|String|Object]}` - The key and value for the next element.
* `prev([opts])` - Gets the previous cursor key and value from the db. Moves the cursor to the previous element.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[Object] {key: [String], value: [Buffer|String|Object]}` - The key and value for the previous element.
* `first([opts])` - Gets the first cursor key and value from the db. Moves the cursor to the first element.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[Object] {key: [String], value: [Buffer|String|Object]}` - The key and value for the first element.
* `last([opts])` - Gets the last cursor key and value from the db. Moves the cursor to the last element.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[Object] {key: [String], value: [Buffer|String|Object]}` - The key and value for the last element.
* `set(key, [opts])` - Moves the element to the key specified and returns the key and value from that position.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[Object] {key: [String], value: [Buffer|String|Object]}` - The key and value for the specified element.
* `put(val, [opts])` - Stores or updates a value at the current cursor key.
  - param: `val` - `[Buffer|String|Object]` - The value to store.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[number]` - 0 if successful, otherwise an error occurred.
* `del([opts])` - Deletes a key.
  - param: `[opts]` - `[Object]` - An [options](#options-object) object.
  - returns `[number]` - 0 if successful, otherwise an error occurred.

## Tests

  `npm test`

## Todo
* Implement Bulk search operations
* Implement DB_SEQUENCE
* Implement DB_LOGC

## Licence

node-berkeleydb is licensed under an MIT license. All rights not explicitly granted in the MIT license are reserved. See the included LICENSE file for more details.
