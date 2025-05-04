#include "alloc.h"
#include <stdio.h>
#include "layers.h"

ARRAY_DEFINE(Layer, LayerArray)

bool layer_new(Layer* layer, const char* name, LayerFn fn, bool can_fail, LayerLC when, void* tag) {
    if (!layer || !name || !fn) return false;

    layer->name = string_new(name, tag);
    if (!layer->name) {
        return false;
    }

    layer->fn = fn;
    layer->when = when;
    layer->can_fail = can_fail;
    return true;
}

void layer_free(Layer* layer) {
    if (!layer) return;
    if (layer->name) {
        string_free(layer->name);
    }
    pfree(layer);
}

bool layer_apply(Layer* layer, HttpRequest* request, HttpResponse* response) {
    if (!layer || !layer->fn) return false;
    return layer->fn(request, response);
}

void layer_print(Layer* layer) {
    if (!layer) return;
    printf("Layer: %s\n", string_cstr(layer->name));
}

LayerCtx* layers_new(void* tag) {
    LayerCtx* ctx = pmalloc(sizeof(LayerCtx), tag);
    if (!ctx) return NULL;

    ctx->layers.tag = tag;
    if (!LayerArray_init(&ctx->layers, tag)) {
        pfree(ctx);
        return NULL;
    }

    return ctx;
}

void layers_free(LayerCtx* ctx) {
    if (!ctx) return;

    for (size_t i = 0; i < LayerArray_size(&ctx->layers); i++) {
        Layer* layer = LayerArray_get_ptr(&ctx->layers, i);
        layer_free(layer);
    }

    LayerArray_destroy(&ctx->layers);
    pfree(ctx);
}

bool layers_add(LayerCtx* ctx, LayerLC when, const char* name, LayerFn fn, bool can_fail) {
    if (!ctx || !name || !fn) return false;

    Layer layer;
    if (!layer_new(&layer, name, fn, can_fail, when, ctx->layers.tag)) {
        return false;
    }

    if (!LayerArray_push(&ctx->layers, layer)) {
        layer_free(&layer);
        return false;
    }

    return true;
}

bool layers_apply(LayerCtx* ctx, LayerLC stage, HttpRequest* request, HttpResponse* response) {
    if (!ctx || !request || !response) return false;

    for (size_t i = 0; i < LayerArray_size(&ctx->layers); i++) {
        Layer* layer = LayerArray_get_ptr(&ctx->layers, i);
        if (layer->when != stage) continue;
        if (!layer_apply(layer, request, response)) {
            if (layer->can_fail) {
                continue;
            } else {
                printf("Layer %s failed\n", string_cstr(layer->name));
                return false;
            }
        }
    }
    return true;
}

void layers_clear(LayerCtx* ctx) {
    if (!ctx) return;

    for (size_t i = 0; i < LayerArray_size(&ctx->layers); i++) {
        Layer* layer = LayerArray_get_ptr(&ctx->layers, i);
        layer_free(layer);
    }

    LayerArray_clear(&ctx->layers);
}

void layers_print(LayerCtx* ctx) {
    if (!ctx) return;

    for (size_t i = 0; i < LayerArray_size(&ctx->layers); i++) {
        Layer* layer = LayerArray_get_ptr(&ctx->layers, i);
        layer_print(layer);
    }
}
