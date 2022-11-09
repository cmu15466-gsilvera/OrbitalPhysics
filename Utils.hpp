#include <stdio.h>
#include <stdlib.h>
#include <time.h>
struct Utils {
	public:
		static float RandBetween(float LO, float HI){
			float val = LO + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(HI-LO)));
			return val;
		};
		static void InitRand(){
			srand((unsigned int)time(NULL));
		};
};
