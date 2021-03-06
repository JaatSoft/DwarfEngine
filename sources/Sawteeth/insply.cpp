#include "insply.h"
#include "song.h"
#include <stdio.h>
#include <math.h>

InsPly::InsPly(Song *s)
{
	song = s;
	vib=new lfo();
	pwm=new lfo();
	w=new Wave();
	ins=&song->ins[0];
	currins=ins;	
	steps=(currins->steps);

	insfreq = currpartfreq = 440.0;
	pwmoffs = amp = cutoff = res = 0.0;
	currpartamp = currpartco = curramp = currf = 0.0;
	lo = hi = bp = bs = ampstep = fstep = 0.0;
	TrigADSR(0);
}

void InsPly::TrigADSR(uint8 i)
{
//	fprintf(stderr,"InsPly::TrigADSR(%d)\n",i);

   pwmoffs = 0.0;
	trigged = true;
	currins = ins+i;
	steps = (currins->steps);

	vib->SetFreq(ins[i].vibs * 50);
	vamp = ins[i].vibd / 2000.0;

	pwm->SetFreq(ins[i].pwms * 5);
	pamp = ins[i].pwmd / 255.1;

	res = 1- ins[i].res / 255.0;
	res *= res;
	res *= res;
	res = 1 - res;

//	amp adsr
	adsr=0;
	nextadsr=0;

//	filter adsr
	fadsr=0;
	nextfadsr=0;

//	step
	stepc=0;
	nexts=0;
}


void InsPly::SetPWMOffs(float a)
{
//	fprintf(stderr,"InsPly::SetPWMOffs(%f)\n",a);
	pwmoffs = a;
}

void InsPly::SetAmp(float a)
{
//	fprintf(stderr,"InsPly::SetAmp(%f) amp=%f\n",a,curramp);
	currpartamp=a;
}

void InsPly::SetFreq(float f)
{
//	fprintf(stderr,"InsPly::SetFreq(%f)\n",f);
	currpartfreq=f;
}

void InsPly::SetCutOff(float co)
{
//	fprintf(stderr,"InsPly::SetCutOff(%f)\n",co);
	currpartco=co;
}

bool InsPly::Next(float *buffer, uint32 count)
{
//fprintf(stderr, "\nInsPly::Next(%p,%ld)\n",buffer,count);
// checkinsstep
	if (nexts < 1){

//fprintf(stderr,"0.steps=%p nexts=%d stepc=%d\n",steps,nexts,stepc);

		currstep = steps + stepc;

		nexts=currins->sps;

//fprintf(stderr,"1.currstep=%p\n",currstep);
		if (currstep->note){
//fprintf(stderr,"2.currstep->note=%d\n",currstep->note);
			if (currstep->relative){
				insfreq = song->R2F[currstep->note];
			}else{
				insfreq = song->N2F[currstep->note];
			}
		}

		if (currstep->wform){
			w->SetForm(currstep->wform);
		}
		stepc++;

		if (stepc >= currins->len){
			stepc = currins->loop;
		}
	}
	
// checkadsr
	if (nextadsr < 1){
		if (adsr < currins->amppoints){
			nextadsr= 1 + currins->amp[adsr].time;
			ampstep= ((currins->amp[adsr].lev/257.0)-curramp) / nextadsr;
			adsr++;
		}
		else
			ampstep=0.0;
	}

// checkfadsr
	if (nextfadsr < 1){
		if (fadsr < currins->filterpoints){
			nextfadsr= 1 + currins->filter[fadsr].time;
			float target = currins->filter[fadsr].lev/257.0;
			target *= target * target;
			fstep= (target-currf) / nextfadsr;
			fadsr++;
		}
		else
			fstep=0.0;
	}


//	adsr
	nextadsr--;
	nextfadsr--;

//	step
	nexts--;

//	Freq
	if (currstep->relative)
		w->SetFreq(currpartfreq * insfreq * (1 +  vamp * vib->Next()));		
	else
		w->SetFreq(insfreq * (1 + vamp * vib->Next()));		

//	PWM
	w->SetPWM(pwmoffs + pamp * pwm->Next());
	
//	Amp
	curramp+=ampstep;
	amp = curramp*currpartamp;
//	Filter
	currf+=fstep;
	cutoff=currf*currpartco;

//	if (fabsf(amp*hi) > 1.0)
//	fprintf(stderr,"insply:\n\tcurramp:%f ampstep:%f cpartamp:%f amp:%f\n\tcurrf  :%f fstep  :%f cpartco :%f co :%f\n",curramp,ampstep,currpartamp,amp,currf,	fstep,	currpartco,	cutoff);


// -----------------------------------------------------------------------------
// filterpart

	w->SetAmp(amp);
	if ( trigged ){
		trigged = false;
		w->NoInterp();
	}
	
	if (! w->Next(buffer, count)) return false;

//fprintf(stderr,"f=%d\ta=%f\n",currins->filtermode,amp);

		
	switch ( currins->filtermode ){
		case 1 : SLP( buffer, count );	break;
		case 2 : OLP( buffer, count );	break;
		case 3 : LP( buffer, count );	break;
		case 4 : HP( buffer, count );	break;
		case 5 : BP( buffer, count );	break;
		case 6 : BS( buffer, count );	break;
	}

	switch ( currins->clipmode ){
		case 0x1 : VanillaClip( buffer, count, 2.0 * (float)(1 + currins->boost));	break;
		case 0x2 : SinusClip( buffer, count, 0.7 * (float)(1.3 + currins->boost));	break;
	}
	
	return true;
};

inline void InsPly::VanillaClip(float *b, uint32 count, float mul)
{
	float *stop = b + count;
	
	if (fabsf(mul - 1.0) < 0.1) // om mul nära ett strunta i mul
		while( b < stop ){
			if ( *b > 1) *b = 1; else if (*b < -1) *b = -1;
			b++;
		}
	else
		while( b < stop ){
			*b *= mul;
			if ( *b > 1) *b = 1; else if (*b < -1) *b = -1;
			b++;
		}
}

inline void InsPly::SinusClip(float *b, uint32 count, float mul)
{
	float *stop = b + count;
	
	if (fabsf(mul - 1.0) < 0.1) // om mul nära ett strunta i mul
		while( b < stop ){
			*b  = sin(*b);
			b++;
		}
	else
		while( b < stop ){
			*b *= mul;
			*b  = sin(*b * mul);
			b++;
		}
}

inline void InsPly::SLP(float *b, uint32 count)
{
	float *stop = b + count;
	float in;
	
	while( b < stop ){
		in = *b;
		lo = (cutoff*in) + (lo*(1-cutoff));
		*b = lo;
		b++;
	}
}


inline void InsPly::OLP(float *b, uint32 count)
{
//	gamla prylen
/*
	curr = currf * amp * w->Next() + (1-currf)*last;
	if (res>0) curr+= (curr-llast)*res;

	llast= last;
	last = curr;
	return curr;
*/
//

	float *stop = b + count;
	
	while( b < stop ){
		lo = cutoff * *b + (1-cutoff)*hi;
		if (res > 0) lo+= (lo-bp)*res;

		bp= hi;
		hi = lo;

		*b = lo;
		b++;
	}
}

inline void InsPly::LP(float *b, uint32 count)
{
	float *stop = b + count;
	float in;
	float t;
	
	while( b < stop ){
		in = *b;
		t=lo+cutoff*bp;
		hi=in-lo-(1.8-res*1.8)*bp;
		bp+=cutoff*hi;
		if (t < -amp) lo = -amp; else if (t > amp) lo = amp; else lo = t;
		bs=lo+hi;
		*b = lo;
		b++;
	}
}

inline void InsPly::HP(float *b, uint32 count)
{
	float *stop = b + count;
	float in;
	float t;
	
	while( b < stop ){
		in = *b;
		t=lo+cutoff*bp;
		hi=in-lo-(1.8-res*1.8)*bp;
		bp+=cutoff*hi;
		if (t < -amp) lo = -amp; else if (t > amp) lo = amp; else lo = t;
		bs=lo+hi;
		*b = hi;
		b++;
	}
}

inline void InsPly::BP(float *b, uint32 count)
{
	float *stop = b + count;
	float in;
	float t;
	
	while( b < stop ){
		in = *b;
		t=lo+cutoff*bp;
		hi=in-lo-(1.8-res*1.8)*bp;
		bp+=cutoff*hi;
		if (t < -amp) lo = -amp; else if (t > amp) lo = amp; else lo = t;
		bs=lo+hi;
		*b = bp;
		b++;
	}
}

inline void InsPly::BS(float *b, uint32 count)
{
	float *stop = b + count;
	float in;
	float t;
	
	while( b < stop ){
		in = *b;
		t=lo+cutoff*bp;
		hi=in-lo-(1.8-res*1.8)*bp;
		bp+=cutoff*hi;
		if (t < -amp) lo = -amp; else if (t > amp) lo = amp; else lo = t;
		bs=lo+hi;
		*b = bs;
		b++;
	}
}
