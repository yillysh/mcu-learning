#include "AI8051U.h" 

void main()
{
	P2M0 |= 0x01;
	P2M1 &= ~0x01;
	while(1)
	{
		P20 = 0;
	}
}