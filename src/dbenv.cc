#include <node.h>
#include <nan.h>

#include "dbenv.h"
#include "dbtxn.h"

#include <cstdlib>
#include <cstring>
#include <uv.h>

using namespace v8;

DbEnv::DbEnv() : _dbenv(0) {};
DbEnv::~DbEnv() {
  close(0);
};

void DbEnv::Init(Local<Object> exports) {
  Nan::HandleScope scope;

  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("DbEnv").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "open", Open);
  Nan::SetPrototypeMethod(tpl, "close", Close);
  // Nan::SetPrototypeMethod(tpl, "txnBegin", TxnBegin);

  exports->Set(Nan::New("DbEnv").ToLocalChecked(), tpl->GetFunction());
}

DB_ENV* DbEnv::get_env() {
  return _dbenv;
}

int DbEnv::create(u_int32_t flags) {
  return db_env_create(&_dbenv, flags);
}

int DbEnv::open(char const *db_home, u_int32_t flags, int mode) {
  return _dbenv->open(_dbenv, db_home, flags, mode);
}

int DbEnv::close(u_int32_t flags) {
  int ret = 0;
  if (_dbenv && _dbenv->lg_size) {
    //fprintf(stderr, "%p: close %p\n", this, _db);
    ret = _dbenv->close(_dbenv, flags);
    _dbenv = NULL;
  }
  return ret;
}

void DbEnv::New(const Nan::FunctionCallbackInfo<Value>& args) {
  DbEnv* env = new DbEnv();
  env->Wrap(args.This());

  int dbFlags((args[0]->IsInt32()) ? args[0]->Int32Value() : 0);

  int ret = env->create(dbFlags);
  if (ret) {
    Nan::ThrowTypeError("Could not create DBENV object");
    return;
  }

  args.GetReturnValue().Set(args.This());
}

void DbEnv::Open(const Nan::FunctionCallbackInfo<Value>& args) {
  if (! args[0]->IsString()) {
    Nan::ThrowTypeError("First argument must be String");
    return;
  }

  DbEnv* env = Nan::ObjectWrap::Unwrap<DbEnv>(args.This());
  String::Utf8Value db_name(args[0]);
  int dbFlags((args[1]->IsInt32()) ? args[1]->Int32Value() : DB_INIT_TXN|DB_INIT_LOG|DB_INIT_LOCK|DB_INIT_MPOOL|DB_CREATE);
  int ret = env->open(*db_name, dbFlags, 0);
  args.GetReturnValue().Set(Nan::New(double(ret)));
}

void DbEnv::Close(const Nan::FunctionCallbackInfo<Value>& args) {
  DbEnv* env = Nan::ObjectWrap::Unwrap<DbEnv>(args.This());
  int ret = env->close(0);
  args.GetReturnValue().Set(Nan::New(double(ret)));
}
