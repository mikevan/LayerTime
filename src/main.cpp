#include <Arduino.h>
#include "app/WatchApp.h"

WatchApp app;

void setup()
{
    app.begin();
}

void loop()
{
    app.tick();
}
