#include "analyze.h"
#include <libavcodec/avfft.h>

#define WIN_BITS 10
#define WIN_SIZE (1 << WIN_BITS)
//#define MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )

struct d2vector envelope_sort(struct song song) {
	struct d2vector result;
	FFTSample *d_freq;
	FFTSample *x;
	RDFTContext *fft;
	int precision = 350;
	int freq_size = WIN_SIZE/2;
	float decr_speed = 1/((float)song.sample_rate*0.45); // Make the envelope converge to zero in 0.45s
	float delta_freq = (float)song.sample_rate/((float)precision*freq_size);
	FILE *file_env;
	size_t rbuf_head = 0;
	int16_t d_envelope = 0;
	int32_t sample_max = (1 << 8*song.nb_bytes_per_sample)/2;
	int64_t atk = 0;
	float final_tempo = 0;
	float final_atk;
	float env, env_prev = 0;
	size_t i, d;
	float period_max1 = 0;
	float period_max2 = 0;
	float period_max3 = 0;
	file_env = fopen("file_env.txt", "w");

	d_freq = (FFTSample*)av_malloc(freq_size*sizeof(FFTSample));

	if(song.nSamples % freq_size > 0)
		song.nSamples -= song.nSamples%freq_size; 

	x = (FFTSample*)av_malloc(WIN_SIZE*sizeof(FFTSample));
	fft = av_rdft_init(WIN_BITS, DFT_R2C);

	for(i = 0; i < freq_size; ++i)
		d_freq[i] = 0.0f;

	for(i = 0; i < WIN_SIZE; ++i)
		x[i] = 0.0f;

	for(i = 0; i < song.nSamples; i++) {	
		env = MAX(env_prev - decr_speed*env_prev, (float)(abs(((int16_t*)song.sample_array)[i])));

		if(i >= precision && i % precision == 0) {
			if((i/precision) % WIN_SIZE != 0) {
				d_envelope += x[(i/precision) % WIN_SIZE - 1] - x[(i/precision) % WIN_SIZE - 2];
				atk += d_envelope > 0 ? d_envelope*d_envelope : 0;
				x[(i/precision) % WIN_SIZE - 1] = env;
			}
			else {
				d_envelope += (x[WIN_SIZE -1] - x[WIN_SIZE-2])/sample_max;
				atk += d_envelope*d_envelope;

				x[WIN_SIZE - 1] = env;
				av_rdft_calc(fft, x);
				for(d = 1; d < freq_size - 1; ++d) {
					float re = x[d*2];
					float im = x[d*2+1];
					float raw = re*re + im*im;
					d_freq[d] += raw;
				}
				d_freq[0] = 0;
			}
		}
		rbuf_head = (rbuf_head+1) % (2*precision);
		env_prev = env;
	}

	for(i = 1; i < freq_size/2; ++i) {
		if(d_freq[(int)period_max1] < d_freq[i]) {
			period_max3 = period_max2;
			period_max2 = period_max1;
			period_max1 = (float)i;
		}
		else if(d_freq[(int)period_max2] < d_freq[i]) {
				period_max3 = period_max2;
				period_max2 = (float)i;
		}
		else if(d_freq[(int)period_max3] < d_freq[i])
			period_max3 = (float)i;
	}
	period_max1++;
	period_max2++;
	period_max3++;

	period_max1 = 1/(period_max1*delta_freq);
	period_max2 = 1/(period_max2*delta_freq);
	period_max3 = 1/(period_max3*delta_freq);

	final_tempo = -6*MIN(MIN(period_max1, period_max2), MAX(period_max2, period_max3)) + 6;

	final_atk = 0.00036*atk/song.nSamples - 183.5;

	if(debug) {
		for(i = 0; i < freq_size; ++i)
			fprintf(file_env, "%f\n", d_freq[i]);
		printf("-> Debug envelope\n");
		printf("Most frequent period: %fs\n", period_max1);
		printf("2nd most frequent period: %fs\n", period_max2);
		printf("3rd most frequent period: %fs\n", period_max3);
		printf("Tempo result: %f\n", final_tempo);
		printf("Attack result: %f\n", final_atk);
	}

	fclose(file_env); 
	result.x = final_tempo;
	result.y = final_atk;
	return result;
}
