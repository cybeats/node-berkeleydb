#ifndef DB_H
#define DB_H

#include <nan.h>

#include <db.h>

class Db : public Nan::ObjectWrap {
 public:
  static void Init(v8::Local<v8::Object> exports);

  DB* get_db();

  int create(DB_ENV *dbenv, u_int32_t flags);
  int open(DB_TXN *txn, char const *fname, char const *db, DBTYPE type, u_int32_t flags, int mode);
  int close(u_int32_t flags);
  int truncate(DB_TXN *txnid, u_int32_t *countp, u_int32_t flags);

  int get(DB_TXN *txn, DBT *key, DBT *data, u_int32_t flags);
  int put(DB_TXN *txn, DBT *key, DBT *data, u_int32_t flags);
  int del(DB_TXN *txn, DBT *key, u_int32_t flags);

  int associate(DB_TXN *txn, DB *sdb, int (*callback)(DB *secondary, const DBT *key, const DBT *data, DBT *result), int flags);
  int set_flags(int flags);

 private:
  Db();
  ~Db();

  DB *_db;
  DB_ENV* get_env();


  static void New(const Nan::FunctionCallbackInfo<v8::Value>& args);
  static void Open(const Nan::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const Nan::FunctionCallbackInfo<v8::Value>& args);
  static void Truncate(const Nan::FunctionCallbackInfo<v8::Value>& args);
  static void Get(const Nan::FunctionCallbackInfo<v8::Value>& args);
  static void Put(const Nan::FunctionCallbackInfo<v8::Value>& args);
  static void Del(const Nan::FunctionCallbackInfo<v8::Value>& args);
  static void Associate(const Nan::FunctionCallbackInfo<v8::Value>& args);
  static void SetFlags(const Nan::FunctionCallbackInfo<v8::Value>& args);
};

#endif
