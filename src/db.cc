#include <node.h>
#include <nan.h>

#include "db.h"
#include "dbenv.h"
#include "dbtxn.h"
#include "util.h"

#include <cstdlib>
#include <cstring>
#include <uv.h>
#include <ffi.h>

using namespace v8;

Db::Db() : _db(0) {};
Db::~Db() {
  close(0);
};

void Db::Init(Local<Object> exports) {
  Nan::HandleScope scope;

  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("Db").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "open", Open);
  Nan::SetPrototypeMethod(tpl, "close", Close);
  Nan::SetPrototypeMethod(tpl, "truncate", Truncate);
  Nan::SetPrototypeMethod(tpl, "_put", Put);
  Nan::SetPrototypeMethod(tpl, "_get", Get);
  Nan::SetPrototypeMethod(tpl, "_del", Del);
  Nan::SetPrototypeMethod(tpl, "associate", Associate);
  Nan::SetPrototypeMethod(tpl, "setFlags", SetFlags);

  exports->Set(Nan::New("Db").ToLocalChecked(), tpl->GetFunction());
}

DB* Db::get_db() {
  return _db;
}

DB_ENV* Db::get_env() {
  return _db->get_env(_db);
}

int Db::create(DB_ENV *dbenv, u_int32_t flags) {
  return db_create(&_db, dbenv, flags);
}

int Db::open(DB_TXN *txnid, char const *fname, char const *db, DBTYPE type, u_int32_t flags, int mode) {
  // fprintf(stderr, "Db::open - txnid = %p\n", txnid);
  return _db->open(_db, txnid, fname, db, type, flags, mode);
}

int Db::close(u_int32_t flags) {
  int ret = 0;
  if (_db && _db->pgsize) {
    //fprintf(stderr, "%p: close %p\n", this, _db);
    ret = _db->close(_db, flags);
    _db = NULL;
  }
  return ret;
}

int Db::truncate(DB_TXN *txn, u_int32_t *countp, u_int32_t flags) {
  return _db->truncate(_db, txn, countp, flags);
}

int Db::put(DB_TXN *txn, DBT *key, DBT *data, u_int32_t flags) {
  // fprintf(stderr, "Db::put - txn = %p\n", txn);
  return _db->put(_db, txn, key, data, flags);
}

int Db::get(DB_TXN *txn, DBT *key, DBT *data, u_int32_t flags) {
  return _db->get(_db, txn, key, data, flags);
}

int Db::del(DB_TXN *txn, DBT *key, u_int32_t flags) {
  return _db->del(_db, txn, key, flags);
}

int Db::associate(DB_TXN *txn, DB *sdb, int (*callback)(DB *secondary, const DBT *key, const DBT *data, DBT *result), int flags) {
  return _db->associate(_db, txn, sdb, callback, flags);
}

int Db::set_flags(int flags) {
  return _db->set_flags(_db, flags);
}


void Db::New(const Nan::FunctionCallbackInfo<Value>& args) {
  Db* store = new Db();
  store->Wrap(args.This());

  DB_ENV* _dbenv = NULL;
  if (args.Length() > 0) {
    if (! args[0]->IsObject()) {
      return Nan::ThrowTypeError("First argument must be a DbEnv object");
    }
    DbEnv* dbenv = Nan::ObjectWrap::Unwrap<DbEnv>(args[0]->ToObject());
    _dbenv = dbenv->get_env();
  }
  
  int dbFlags((args[1]->IsInt32()) ? args[1]->Int32Value() : 0);

  int ret = store->create(_dbenv, dbFlags);

  if (ret) {
    return Nan::ThrowTypeError("Could not create DB object");
  }

  args.GetReturnValue().Set(args.This());
}

void Db::Open(const Nan::FunctionCallbackInfo<Value>& args) {
  if (! args[0]->IsString()) {
    Nan::ThrowTypeError("First argument must be a String (path to db)");
    return;
  }

  Db* store = Nan::ObjectWrap::Unwrap<Db>(args.This());
  DB_TXN *tid = NULL;

  DB_ENV *env = store->get_env();
  if (env) {
    env->txn_begin(env, NULL, &tid, 0);
  }
  String::Utf8Value fname(args[0]);
  String::Utf8Value db(args[1]);
  DBTYPE dbType = static_cast<DBTYPE>((args[2]->IsInt32()) ? args[2]->Int32Value() : DB_BTREE);
  int dbFlags((args[3]->IsInt32()) ? args[3]->Int32Value() : DB_CREATE);

  int ret = store->open(tid, *fname, (args[1]->IsString()) ? *db : NULL, dbType, dbFlags, 0);
  if (tid) {
    tid->commit(tid, 0);
  }
  args.GetReturnValue().Set(Nan::New(double(ret)));
}

void Db::Close(const Nan::FunctionCallbackInfo<Value>& args) {
  Db* store = Nan::ObjectWrap::Unwrap<Db>(args.This());
  int ret = store->close(0);
  args.GetReturnValue().Set(Nan::New(double(ret)));
}

void Db::Truncate(const Nan::FunctionCallbackInfo<Value>& args) {
  Db* store = Nan::ObjectWrap::Unwrap<Db>(args.This());
  DB_TXN *tid = NULL;
  db_recno_t *countp = NULL;

  DB_ENV *env = store->get_env();
  if (env) {
    env->txn_begin(env, NULL, &tid, 0);
  }
  int ret = store->truncate(tid, countp, 0);
  if (tid && !ret) {
    tid->commit(tid, 0);
  }
  args.GetReturnValue().Set(Nan::New(u_int32_t(u_long(&countp))));
}

void Db::Put(const Nan::FunctionCallbackInfo<Value>& args) {
  // fprintf(stderr, "Db::Put (start) - args[2] = %p\n", *args[2]);
  DB_TXN * dbtxn = NULL;

  if (!(args.Length() > 0) && ! args[0]->IsString()) {
    Nan::ThrowTypeError("First argument must be a String (key)");
    return;
  }

  if (!(args.Length() > 1) && ! node::Buffer::HasInstance(args[1])) {
    Nan::ThrowTypeError("Second argument must be a Buffer (value)");
    return;
  }

  if (args.Length() > 2) {
    if (args[2]->IsObject()) {
      Local<Object> wrapped = args[2]->ToObject();
      DbTxn *_dbtxn = Nan::ObjectWrap::Unwrap<DbTxn>(wrapped);
      dbtxn = _dbtxn->get_txn();
    }
    // fprintf(stderr, "Db::Put - dbtxn = %p, _ = %p (wrapped %p)\n", dbtxn, _dbtxn, *wrapped);
  }
  Db* store = Nan::ObjectWrap::Unwrap<Db>(args.This());

  String::Utf8Value key(args[0]);
  Local<Object> buf = args[1]->ToObject();
  int dbFlags((args[3]->IsInt32()) ? args[3]->Int32Value() : 0);
  
  DBT * key_dbt = new DBT();
  dbt_set(key_dbt, *key, strlen(*key));
  DBT * data_dbt = new DBT();
  dbt_set(data_dbt,
          node::Buffer::Data(buf),
          node::Buffer::Length(buf));

  int ret = store->put(dbtxn, key_dbt, data_dbt, dbFlags);

  args.GetReturnValue().Set(Nan::New(double(ret)));
}

void Db::Get(const Nan::FunctionCallbackInfo<Value>& args) {
  DB_TXN * dbtxn = NULL;

  if (args.Length() == 0 || ! args[0]->IsString()) {
    Nan::ThrowTypeError("First argument must be a String (key)");
    return;
  }

  if (args.Length() > 1) {
    if (args[1]->IsObject()) {
      DbTxn *_dbtxn = Nan::ObjectWrap::Unwrap<DbTxn>(args[1]->ToObject());
      dbtxn = _dbtxn->get_txn();
    }
  }

  Db* store = Nan::ObjectWrap::Unwrap<Db>(args.This());
  int dbFlags((args[2]->IsInt32()) ? args[2]->Int32Value() : 0);

  String::Utf8Value key(args[0]);

  DBT key_dbt;
  dbt_set(&key_dbt, *key, strlen(*key));
  DBT retbuf;
  dbt_set(&retbuf, 0, 0, DB_DBT_MALLOC);

  store->get(dbtxn, &key_dbt, &retbuf, dbFlags);

  // fprintf(stderr, "get = %p\n", &key_dbt);
  // fprintf(stderr, "get = %d\n", sizeof(retbuf));

  // __builtin_dump_struct(&key_dbt, &printf);
  // __builtin_dump_struct(&retbuf, &printf);

  Local<Object> buf = Nan::NewBuffer((char*)retbuf.data, retbuf.size, free_buf, NULL).ToLocalChecked();

  args.GetReturnValue().Set(buf);
}

void Db::Del(const Nan::FunctionCallbackInfo<Value>& args) {
  DB_TXN * dbtxn = NULL;

  if (!(args.Length() > 0) && ! args[0]->IsString()) {
    Nan::ThrowTypeError("First argument must be a String (key)");
    return;
  }

  if (args.Length() > 1) {
    if (args[1]->IsObject()) {
      DbTxn *_dbtxn = Nan::ObjectWrap::Unwrap<DbTxn>(args[1]->ToObject());
      dbtxn = _dbtxn->get_txn();
    }
  }

  Db* store = Nan::ObjectWrap::Unwrap<Db>(args.This());
  int dbFlags((args[2]->IsInt32()) ? args[2]->Int32Value() : 0);

  String::Utf8Value key(args[0]);
  
  DBT key_dbt;
  dbt_set(&key_dbt, *key, strlen(*key));
  
  int ret = store->del(dbtxn, &key_dbt, dbFlags);

  args.GetReturnValue().Set(Nan::New(double(ret)));
}

void callback_binding(ffi_cif *cif, void *ret, void* args[], void *function)
{
  // DB *sdb = (DB*)(*((void **)args[0]));
  DBT *key = (DBT*)(*((void **)args[1]));
  DBT *data = (DBT*)(*((void **)args[2]));
  DBT *result = (DBT*)(*((void **)args[3]));
  Nan::Callback *callback = (Nan::Callback *)(function);

  // fprintf(stderr, "callback_binding = %p\n", args[0]);
  // fprintf(stderr, "callback_binding = %p\n", args[1]);
  // fprintf(stderr, "callback_binding = %p\n", args[2]);
  // fprintf(stderr, "callback_binding = %p\n", args[3]);

  // __builtin_dump_struct(key, &printf);
  // __builtin_dump_struct(data, &printf);
  // __builtin_dump_struct(result, &printf);

  // String::Utf8Value ((char*)key->data);
  Isolate* isolate = Nan::GetCurrentContext()->Global()->GetIsolate();
  Local<String> keyStr = String::NewFromUtf8(isolate, (char*)(key->data));

  Local<Value> arguments[2] = {
    keyStr,
    Nan::NewBuffer((char*)data->data, data->size, free_buf, NULL).ToLocalChecked()
  };

  Local<Object> buf = callback->Call(2, arguments)->ToObject();
  
  dbt_set(result, node::Buffer::Data(buf), node::Buffer::Length(buf));

  *(ffi_arg *)ret = 0;
}

void Db::Associate(const Nan::FunctionCallbackInfo<Value>& args) {
  DB_TXN * dbtxn = NULL;
  DB * sdb = NULL;

  if (!(args.Length() > 0) || !(args[0]->IsObject())) {
    Nan::ThrowTypeError("First argument must be a DbTxn");
    return;
  }

  if (!(args.Length() > 1) || !(args[1]->IsObject())) {
    Nan::ThrowTypeError("Second argument must be a Db object (secondary)");
    return;
  }

  if (!(args.Length() > 2) || !(args[2]->IsFunction())) {
    Nan::ThrowTypeError("Third argument must be a function");
    return;
  }

  Db* store = Nan::ObjectWrap::Unwrap<Db>(args.This());
  
  DbTxn *_dbtxn = Nan::ObjectWrap::Unwrap<DbTxn>(args[0]->ToObject());
  dbtxn = _dbtxn->get_txn();

  Db *_sdb = Nan::ObjectWrap::Unwrap<Db>(args[1]->ToObject());
  sdb = _sdb->get_db();
  
  Nan::Callback *callback = new Nan::Callback(Local<Function>::Cast(args[2]));

  int dbFlags((args[3]->IsInt32()) ? args[3]->Int32Value() : DB_CREATE);

  ffi_cif *cif = new ffi_cif;
  ffi_type **ffiargs = (ffi_type **)malloc(sizeof(ffi_type)*4);

  void *bound_callback;
  int ret = 0;

  ffi_closure *closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), &bound_callback);

  if (closure) {
    ffiargs[0] = &ffi_type_pointer;
    ffiargs[1] = &ffi_type_pointer;
    ffiargs[2] = &ffi_type_pointer;
    ffiargs[3] = &ffi_type_pointer;

    if (ffi_prep_cif(cif, FFI_DEFAULT_ABI, 4, &ffi_type_uint32, ffiargs) == FFI_OK) {
      if (ffi_prep_closure_loc(closure, cif, callback_binding, callback, bound_callback) == FFI_OK) {
        ret = store->associate(dbtxn, sdb, (int (*)(DB *, const DBT *, const DBT *, DBT *))(bound_callback), dbFlags);
      }
    }
  }
  args.GetReturnValue().Set(Nan::New(double(ret)));
}

void Db::SetFlags(const Nan::FunctionCallbackInfo<v8::Value>& args) {
  if (!(args.Length() > 0) || !(args[0]->IsInt32())) {
    Nan::ThrowTypeError("First argument must be an int");
    return;
  }
  
  Db* store = Nan::ObjectWrap::Unwrap<Db>(args.This());
  int dbFlags(args[0]->Int32Value());
  int ret = store->set_flags(dbFlags);
  
  args.GetReturnValue().Set(Nan::New(double(ret)));
}