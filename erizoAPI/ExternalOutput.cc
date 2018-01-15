#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "lib/json.hpp"
#include "ExternalOutput.h"

using v8::HandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Value;
using json = nlohmann::json;

Nan::Persistent<Function> ExternalOutput::constructor;

class AsyncCloser : public Nan::AsyncWorker {
 public:
    AsyncCloser(std::shared_ptr<erizo::ExternalOutput> external_output, Nan::Callback *callback):
      AsyncWorker(callback), external_output_(external_output) {
      }
    ~AsyncCloser() {}
    void Execute() {
      external_output_->close();
    }
    void HandleOKCallback() {
      Nan::HandleScope scope;
      std::string msg("OK");
      if (callback) {
        Local<Value> argv[] = {
          Nan::New(msg.c_str()).ToLocalChecked()
        };
        callback->Call(1, argv);
      }
    }
 private:
    std::shared_ptr<erizo::ExternalOutput> external_output_;
};

ExternalOutput::ExternalOutput() {}
ExternalOutput::~ExternalOutput() {}

NAN_MODULE_INIT(ExternalOutput::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ExternalOutput").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", init);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("ExternalOutput").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(ExternalOutput::New) {
  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string url = std::string(*param);
  v8::String::Utf8Value json_param(Nan::To<v8::String>(info[1]).ToLocalChecked());
  std::string media_config_string = std::string(*json_param);
  json media_config = json::parse(media_config_string);
  std::vector<erizo::RtpMap> rtp_mappings;
  if (media_config.find("rtpMappings") != media_config.end()) {
    json rtp_map_json = media_config["rtpMappings"];
    for (json::iterator it = rtp_map_json.begin(); it != rtp_map_json.end(); ++it) {
      erizo::RtpMap rtp_map;
      if (it.value()["payloadType"].is_number()) {
        rtp_map.payload_type = it.value()["payloadType"];
      } else {
        continue;
      }
      if (it.value()["encodingName"].is_string()) {
        rtp_map.encoding_name = it.value()["encodingName"];
      } else {
        continue;
      }
      if (it.value()["mediaType"].is_string()) {
        if (it.value()["mediaType"] == "video") {
          rtp_map.media_type = erizo::VIDEO_TYPE;
        } else if (it.value()["mediaType"] == "audio") {
          rtp_map.media_type = erizo::AUDIO_TYPE;
        } else {
          continue;
        }
      } else {
        continue;
      }
      if (it.value()["clockRate"].is_number()) {
        rtp_map.clock_rate = it.value()["clockRate"];
      }
      if (rtp_map.media_type == erizo::AUDIO_TYPE) {
        if (it.value()["channels"].is_number()) {
          rtp_map.channels = it.value()["channels"];
        }
      }
      if (it.value()["formatParameters"].is_object()) {
        json format_params_json = it.value()["formatParameters"];
        for (json::iterator params_it = format_params_json.begin();
            params_it != format_params_json.end(); ++params_it) {
          std::string value = params_it.value();
          std::string key = params_it.key();
          rtp_map.format_parameters.insert(rtp_map.format_parameters.begin(),
              std::pair<std::string, std::string> (key, value));
        }
      }
      if (it.value()["feedbackTypes"].is_array()) {
        json feedback_types_json = it.value()["feedbackTypes"];
        for (json::iterator feedback_it = feedback_types_json.begin();
            feedback_it != feedback_types_json.end(); ++feedback_it) {
            rtp_map.feedback_types.push_back(*feedback_it);
        }
      }
      rtp_mappings.push_back(rtp_map);
    }
  }

  ExternalOutput* obj = new ExternalOutput();
  obj->me = std::make_shared<erizo::ExternalOutput>(url, rtp_mappings);

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(ExternalOutput::close) {
  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(info.Holder());

  Nan::Callback *callback;
  if (info.Length() >= 1) {
    callback = new Nan::Callback(info[0].As<Function>());
  } else {
    callback = NULL;
  }

  Nan::AsyncQueueWorker(new  AsyncCloser(obj->me, callback));
}

NAN_METHOD(ExternalOutput::init) {
  // TODO(pedro) Could potentially be slow, think about async'ing it
  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(info.Holder());
  std::shared_ptr<erizo::ExternalOutput> me = obj->me;

  int r = me->init();
  info.GetReturnValue().Set(Nan::New(r));
}
