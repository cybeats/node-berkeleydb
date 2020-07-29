var addon = require("bindings")("addon.node");

// helpers
function decode(buf, opts={}) {
  if (typeof opts == 'string') {
    opts = { encoding: opts };
  }
  if (opts.encoding || opts.json) {
    buf = buf.toString(opts.encoding || 'utf8');
  }
  if (opts.json) {
    buf = JSON.parse(buf || null);
  }
  return buf;
};

function encode(val, opts={}) {
  if (typeof opts == 'string') {
    opts = { encoding: opts };
  }
  if (opts.json) {
    val = JSON.stringify(val);
  }

  var buf = val;
  if (typeof buf == 'string') {
    buf = new Buffer(val, opts.encoding || 'utf8');
  }
  return buf;
}

function appendOptions(args, opts={}) {
  var newArgs = args.concat(opts.txn ? opts.txn : null);
  return opts.flags ? newArgs.concat(opts.flags) : newArgs;
}

// load refs to objects to customize
var Db = addon.Db;
var DbCursor = addon.DbCursor;


// db
Db.prototype.put = function (key, val, opts={}) {
  key = key.toString('utf8');
  val = encode(val, opts);
  return this._put.apply(this, appendOptions([key, val], opts));
};

Db.prototype.del = function (key, opts={}) {
  key = key.toString('utf8');
  return this._del.apply(this, appendOptions([key], opts));
};

Db.prototype.get = function (key, opts={}) {
  key = key.toString('utf8');
  var val = this._get.apply(this, appendOptions([key], opts));
  return decode(val, opts);
};


// dbcursor
DbCursor.prototype.put = function (val, opts={}) {
  val = encode(val, opts);
  return this._put(val);
};

DbCursor.prototype.del = function () {
  return this._del();
};

DbCursor.prototype.set = function (key, opts={}) {
  key = key.toString('utf8');
  var res = this._set(key);
  var buf = decode(res.value);
  return {key, value: buf};
};

DbCursor.prototype.current = function (opts={}) {
  var res = this._current();
  var value = decode(res.value, opts);
  var key = res.key ? res.key.toString('utf8') : res.key;
  return {key, value};
}

DbCursor.prototype.next = function (opts={}) {
  var res = this._next();
  var value = decode(res.value, opts);
  var key = res.key ? res.key.toString('utf8') : res.key;
  return {key, value};
}

DbCursor.prototype.prev = function (opts={}) {
  var res = this._prev();
  var value = decode(res.value, opts);
  var key = res.key ? res.key.toString('utf8') : res.key;
  return {key, value};
}

DbCursor.prototype.first = function (opts={}) {
  var res = this._first();
  var value = decode(res.value, opts);
  var key = res.key ? res.key.toString('utf8') : res.key;
  return {key, value};
}

DbCursor.prototype.last = function (opts={}) {
  var res = this._last();
  var value = decode(res.value, opts);
  var key = res.key ? res.key.toString('utf8') : res.key;
  return {key, value};
}


module.exports = addon;
