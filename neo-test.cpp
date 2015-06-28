#include "ws2812b.h"

int main(int argc, char **argv){
	
	ws2812b *_ws2812b = new ws2812b(1); //1 pixel LED
    _ws2812b->initHardware();
    _ws2812b->clearLEDBuffer();

    int tmp;

    for(;;){
		//RGB Blink.
		_ws2812b->setPixelColor(0, 255, 0, 0);
		_ws2812b->show();
		usleep(1000*1000);
		
		_ws2812b->setPixelColor(0, 0, 255, 0);
		_ws2812b->show();
		usleep(1000*1000);
		
		_ws2812b->setPixelColor(0, 0, 0, 255);
		_ws2812b->show();
		usleep(1000*1000);

		//Rainbow
        for( int i=0 ; i<=255 ; i++){
            if( i < 85 ){
                _ws2812b->setPixelColor(0, i*3, 255-i*3, 0);
                _ws2812b->show();
            }else if( i < 170 ){
                tmp = i-85;
                _ws2812b->setPixelColor(0, 255-tmp*3, 0, tmp*3);
                _ws2812b->show();
            }else{
                tmp = i-170;
                _ws2812b->setPixelColor(0, 0, tmp*3, 255-tmp*3);
                _ws2812b->show();
            }
            usleep(1000);
        }
		
		usleep(1000*1000);

    }
	
	delete _ws2812b;

    return 0;
}