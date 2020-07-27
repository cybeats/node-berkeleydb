//#define BUILDING_NODE_EXTENSION
#include <nan.h>

#include "db.h"
#include "dbenv.h"
#include "dbtxn.h"
#include "dbcursor.h"
#include "flags.h"

using namespace v8;

void Init(Local<Object> exports) {
  Db::Init(exports);
  DbEnv::Init(exports);
  DbTxn::Init(exports);
  DbCursor::Init(exports);
  Flags::Init(exports);
}

NODE_MODULE(addon, Init)
