/* FastLED_RGBW
 * 
 * Hack to enable SK6812/WS281x RGBW strips to work with FastLED.
 *
 * Original code by Jim Bumgardner (http://krazydad.com).
 * Modified by David Madison (http://partsnotincluded.com).
 * Modified by Bas van Ritbergen <bas.vanritbergen@adyen.com> <bas@ritbit.com>
 * 
*/

#ifndef FastLED_RGBW_h
#define FastLED_RGBW_h

struct CRGBW  {
	union {
		struct {
			union {
				uint8_t g;
				uint8_t green;
			};
			union {
				uint8_t r;
				uint8_t red;
			};
			union {
				uint8_t b;
				uint8_t blue;
			};
			union {
				uint8_t w;
				uint8_t white;
			};
		};
		uint8_t raw[4];
	};

	CRGBW(){}

	CRGBW(uint8_t rd, uint8_t grn, uint8_t blu, uint8_t wht){
		r = rd;
		g = grn;
		b = blu;
		w = wht;
	}

	inline void operator = (const CRGB c) __attribute__((always_inline)){ 
    // If the color is grayscale, treat it as a white value.
    if (c.r == c.g && c.r == c.b) {
      this->r = 0;
      this->g = 0;
      this->b = 0;
      this->white = c.r;
    } else {
      this->r = c.r;
      this->g = c.g;
      this->b = c.b;
      this->white = 0;
    }
	}
};

inline uint16_t getRGBWsize(uint16_t nleds){
  // Calculate the number of CRGB structs needed to hold nleds of CRGBW data.
  return (nleds * 4 + 2) / 3;
}

#endif
