#undef NDEBUG

#ifdef IMGUI_IMPL_API
#undef IMGUI_IMPL_API
#endif
#ifdef __cplusplus
#define IMGUI_IMPL_API extern "C"
#else
#define IMGUI_IMPL_API extern
#endif