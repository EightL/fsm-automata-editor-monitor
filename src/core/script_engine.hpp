// script_engine.hpp
#pragma once
#include <QJSEngine>
#include "context.hpp"

namespace core_fsm::script {
  // shared engine instance
  QJSEngine& engine();

  // bind C++ Context into JS: ctx.inputs, ctx.vars, ctx.outputs, ctx.since
  void bindCtx(QJSEngine& eng, Context& ctx);

  // after running, copy back JS vars & outputs into C++ Context
  void pullBack(QJSEngine& eng, Context& ctx);
}
