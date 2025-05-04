#ifndef LAYERS_H
#define LAYERS_H

#include "http.h"
#include "array.h"

typedef bool (*LayerFn)(HttpRequest* request, HttpResponse* response);

typedef enum {
    LAYER_INIT,
    LAYER_PRE_ROUTE,
    LAYER_POST_ROUTE,
    LAYER_PRE_RESPONSE,
    LAYER_POST_RESPONSE,
    LAYER_CLEANUP,
} LayerLC;

typedef struct {
    String* name;
    LayerLC when;
    LayerFn fn;
    bool can_fail;
} Layer;

ARRAY_DECLARE(Layer, LayerArray)

typedef struct {
    LayerArray layers;
} LayerCtx;


bool layer_new(Layer* layer, const char* name, LayerFn fn, bool can_fail, LayerLC when, void* tag);
void layer_free(Layer* layer);
bool layer_apply(Layer* layer, HttpRequest* request, HttpResponse* response);
void layer_print(Layer* layer);

LayerCtx* layers_new(void* tag);
void layers_free(LayerCtx* ctx);
bool layers_add(LayerCtx* ctx, LayerLC when, const char* name, LayerFn fn, bool can_fail);
bool layers_apply(LayerCtx* ctx, LayerLC stage, HttpRequest* request, HttpResponse* response);
void layers_clear(LayerCtx* ctx);
void layers_print(LayerCtx* ctx);

#endif // LAYERS_H
