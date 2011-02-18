/*
Copyright (c) 2011, Tim Branyen @tbranyen <tim@tabdeveloper.com>
*/

#include <v8.h>
#include <node.h>
#include <node_events.h>

#include "repo.h"

using namespace v8;
using namespace node;

void Repo::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Repo"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "open", Open);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "free", Free);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "init", Init);

  target->Set(String::NewSymbol("Repo"), constructor_template->GetFunction());
}

int Repo::Open(const char* path) {
  return git_repository_open(&this->repo, path);
}

void Repo::Free() {
  git_repository_free(this->repo);
}

int Repo::Init(const char* path, bool is_bare) {
  git_repository* repo_;
  int err = git_repository_init(&repo_, path, is_bare);

  if(err == 0) {
    this->repo = *&repo_;
  }

  return err;
}

Handle<Value> Repo::New(const Arguments& args) {
  HandleScope scope;

  Repo *repo = new Repo();
  repo->Wrap(args.This());

  return args.This();
}

Handle<Value> Repo::Open(const Arguments& args) {
  Repo *repo = ObjectWrap::Unwrap<Repo>(args.This());
  Local<Function> callback;

  HandleScope scope;

  if(args.Length() == 0 || !args[0]->IsString()) {
    return ThrowException(Exception::Error(String::New("Path is required and must be a String.")));
  }

  if(args.Length() == 1 || !args[1]->IsFunction()) {
    return ThrowException(Exception::Error(String::New("Callback must be a Function.")));
  }

  callback = Local<Function>::Cast(args[1]);

  open_request *ar = new open_request();
  ar->repo = repo;
  ar->path = Persistent<Value>::New( args[0] );
  ar->callback = Persistent<Function>::New(callback);

  repo->Ref();

  eio_custom(EIO_Open, EIO_PRI_DEFAULT, EIO_AfterOpen, ar);
  ev_ref(EV_DEFAULT_UC);

  return Undefined();
}

int Repo::EIO_Open(eio_req *req) {
  open_request *ar = static_cast<open_request *>(req->data);

  String::Utf8Value path(ar->path);
  ar->err = Persistent<Value>::New( Integer::New(ar->repo->Open(*path)) );

  return 0;
}

int Repo::EIO_AfterOpen(eio_req *req) {
  HandleScope scope;

  open_request *ar = static_cast<open_request *>(req->data);
  ev_unref(EV_DEFAULT_UC);
  ar->repo->Unref();

  Local<Value> argv[2];
  argv[0] = Number::Cast(*ar->err);
  argv[1] = String::Cast(*ar->path);

  TryCatch try_catch;

  ar->callback->Call(Context::GetCurrent()->Global(), 2, argv);

  if(try_catch.HasCaught())
    FatalException(try_catch);
    
  ar->err.Dispose();
  ar->path.Dispose();
  ar->callback.Dispose();

  delete ar;

  return 0;
}

Handle<Value> Repo::Free(const Arguments& args) {
  Repo *repo = ObjectWrap::Unwrap<Repo>(args.This());

  HandleScope scope;

  repo->Free();

  return Undefined();
}

Handle<Value> Repo::Init(const Arguments& args) {
  Repo *repo = ObjectWrap::Unwrap<Repo>(args.This());
  Local<Function> callback;

  HandleScope scope;

  if(args.Length() == 0 || !args[0]->IsString()) {
    return ThrowException(Exception::Error(String::New("path is required and must be a String.")));
  }

  if(args.Length() == 1 || !args[1]->IsBoolean()) {
    return ThrowException(Exception::Error(String::New("is_bare is required and must be a String.")));
  }

  if(args.Length() == 2 || !args[2]->IsFunction()) {
    return ThrowException(Exception::Error(String::New("Callback must be a Function.")));
  }

  callback = Local<Function>::Cast(args[2]);

  init_request *ar = new init_request();
  ar->repo = repo;
  ar->path = Persistent<Value>::New( args[0] );
  ar->is_bare = Persistent<Boolean>::New( args[1]->ToBoolean() );
  ar->callback = Persistent<Function>::New(callback);

  repo->Ref();

  eio_custom(EIO_Init, EIO_PRI_DEFAULT, EIO_AfterInit, ar);
  ev_ref(EV_DEFAULT_UC);

  return Undefined();
}

int Repo::EIO_Init(eio_req *req) {
  init_request *ar = static_cast<init_request *>(req->data);

  String::Utf8Value path(ar->path);
  Local<Boolean> is_bare = ar->is_bare->ToBoolean();
  ar->err = Persistent<Value>::New( Integer::New(ar->repo->Init(*path, *is_bare)) );

  return 0;
}

int Repo::EIO_AfterInit(eio_req *req) {
  HandleScope scope;

  init_request *ar = static_cast<init_request *>(req->data);
  ev_unref(EV_DEFAULT_UC);
  ar->repo->Unref();

  Local<Value> argv[2];
  argv[0] = Number::Cast(*ar->err);
  argv[1] = String::Cast(*ar->path);
  // Todo: Figure out how to send back boolean
  //argv[2] = Boolean::New(*ar->is_bare);


  TryCatch try_catch;

  ar->callback->Call(Context::GetCurrent()->Global(), 2, argv);

  if(try_catch.HasCaught())
    FatalException(try_catch);
    
  ar->err.Dispose();
  ar->path.Dispose();
  ar->is_bare.Dispose();
  ar->callback.Dispose();

  delete ar;

  return 0;
}

Persistent<FunctionTemplate> Repo::constructor_template;
