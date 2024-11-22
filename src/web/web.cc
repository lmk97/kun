#include "web/web.h"

#include "web/abort_controller.h"
#include "web/abort_signal.h"
#include "web/console.h"
#include "web/dom_exception.h"
#include "web/event.h"
#include "web/event_target.h"
#include "web/text_decoder.h"
#include "web/text_encoder.h"
#include "web/timers.h"

KUN_V8_USINGS;

namespace kun::web {

void expose(Local<Context> context, ExposedScope exposedScope) {
    exposeEventTarget(context, exposedScope);
    exposeAbortController(context, exposedScope);
    exposeAbortSignal(context, exposedScope);
    exposeConsole(context, exposedScope);
    exposeDOMException(context, exposedScope);
    exposeEvent(context, exposedScope);
    exposeTextDecoder(context, exposedScope);
    exposeTextEncoder(context, exposedScope);
    exposeTimers(context, exposedScope);
}

}
