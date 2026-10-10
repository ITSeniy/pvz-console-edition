// Clang 20 emits the C23 roundevenf symbol, which Windows UCRT does not export.
#include <smmintrin.h>

extern "C" __attribute__((target("sse4.1"))) float roundevenf(float value) {
  const __m128 input = _mm_set_ss(value);
  return _mm_cvtss_f32(_mm_round_ss(input, input,
      _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
}
