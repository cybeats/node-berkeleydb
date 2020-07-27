#ifndef FLAGS_H
#define FLAGS_H

#include <nan.h>

class Flags : public Nan::ObjectWrap {
 public:
  static void Init(v8::Local<v8::Object> exports);
};

#endif
