/*
gcc -o x_pigpiod_if x_pigpiod_if.c -lpigpiod_if -lrt -lpthread
sudo ./x_pigpiod_if
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "pigpiod_if.h"

#define GPIO 4

void CHECK(int t, int st, int got, int expect, int pc, char *desc)
{
   if ((got >= (((1E2-pc)*expect)/1E2)) && (got <= (((1E2+pc)*expect)/1E2)))
   {
      printf("TEST %2d.%-2d PASS (%s: %d)\n", t, st, desc, expect);
   }
   else
   {
      fprintf(stderr,
              "TEST %2d.%-2d FAILED got %d (%s: %d)\n",
              t, st, got, desc, expect);
   }
}

void t0()
{
   printf("Version.\n");

   printf("pigpio version %d.\n", get_pigpio_version());

   printf("Hardware revision %d.\n", get_hardware_revision());
}

void t1()
{
   int v;

   printf("Mode/PUD/read/write tests.\n");

   set_mode(GPIO, PI_INPUT);
   v = get_mode(GPIO);
   CHECK(1, 1, v, 0, 0, "set mode, get mode");

   set_pull_up_down(GPIO, PI_PUD_UP);
   v = gpio_read(GPIO);
   CHECK(1, 2, v, 1, 0, "set pull up down, read");

   set_pull_up_down(GPIO, PI_PUD_DOWN);
   v = gpio_read(GPIO);
   CHECK(1, 3, v, 0, 0, "set pull up down, read");

   gpio_write(GPIO, PI_LOW);
   v = get_mode(GPIO);
   CHECK(1, 4, v, 1, 0, "write, get mode");

   v = gpio_read(GPIO);
   CHECK(1, 5, v, 0, 0, "read");

   gpio_write(GPIO, PI_HIGH);
   v = gpio_read(GPIO);
   CHECK(1, 6, v, 1, 0, "write, read");
}

int t2_count=0;

void t2cb(unsigned gpio, unsigned level, uint32_t tick)
{
   t2_count++;
}

void t2()
{
   int f, r, rr, oc;

   printf("PWM dutycycle/range/frequency tests.\n");

   set_PWM_range(GPIO, 255);
   set_PWM_frequency(GPIO, 0);
   f = get_PWM_frequency(GPIO);
   CHECK(2, 1, f, 10, 0, "set PWM range, set/get PWM frequency");

   callback(GPIO, EITHER_EDGE, t2cb);

   set_PWM_dutycycle(GPIO, 0);
   time_sleep(0.5); /* allow old notifications to flush */
   oc = t2_count;
   time_sleep(2);
   f = t2_count - oc;
   CHECK(2, 2, f, 0, 0, "set PWM dutycycle, callback");

   set_PWM_dutycycle(GPIO, 128);
   oc = t2_count;
   time_sleep(2);
   f = t2_count - oc;
   CHECK(2, 3, f, 40, 5, "set PWM dutycycle, callback");

   set_PWM_frequency(GPIO, 100);
   f = get_PWM_frequency(GPIO);
   CHECK(2, 4, f, 100, 0, "set/get PWM frequency");

   oc = t2_count;
   time_sleep(2);
   f = t2_count - oc;
   CHECK(2, 5, f, 400, 1, "callback");

   set_PWM_frequency(GPIO, 1000);
   f = get_PWM_frequency(GPIO);
   CHECK(2, 6, f, 1000, 0, "set/get PWM frequency");

   oc = t2_count;
   time_sleep(2);
   f = t2_count - oc;
   CHECK(2, 7, f, 4000, 1, "callback");

   r = get_PWM_range(GPIO);
   CHECK(2, 8, r, 255, 0, "get PWM range");

   rr = get_PWM_real_range(GPIO);
   CHECK(2, 9, rr, 200, 0, "get PWM real range");

   set_PWM_range(GPIO, 2000);
   r = get_PWM_range(GPIO);
   CHECK(2, 10, r, 2000, 0, "set/get PWM range");

   rr = get_PWM_real_range(GPIO);
   CHECK(2, 11, rr, 200, 0, "get PWM real range");

   set_PWM_dutycycle(GPIO, 0);
}

int t3_reset=1;
int t3_count=0;
uint32_t t3_tick=0;
float t3_on=0.0;
float t3_off=0.0;

void t3cbf(unsigned gpio, unsigned level, uint32_t tick)
{
   uint32_t td;

   if (t3_reset)
   {
      t3_count = 0;
      t3_on = 0.0;
      t3_off = 0.0;
      t3_reset = 0;
   }
   else
   {
      td = tick - t3_tick;

      if (level == 0) t3_on += td;
      else            t3_off += td;
   }

   t3_count ++;
   t3_tick = tick;
}

void t3()
{
   int pw[3]={500, 1500, 2500};
   int dc[4]={20, 40, 60, 80};

   int f, rr;
   float on, off;

   int t;

   printf("PWM/Servo pulse accuracy tests.\n");

   callback(GPIO, EITHER_EDGE, t3cbf);

   for (t=0; t<3; t++)
   {
      set_servo_pulsewidth(GPIO, pw[t]);
      time_sleep(1);
      t3_reset = 1;
      time_sleep(4);
      on = t3_on;
      off = t3_off;
      CHECK(3, 1+t, (1000.0*(on+off))/on, 20000000.0/pw[t], 1,
         "set servo pulsewidth");
   }

   set_servo_pulsewidth(GPIO, 0);
   set_PWM_frequency(GPIO, 1000);
   f = get_PWM_frequency(GPIO);
   CHECK(3, 4, f, 1000, 0, "set/get PWM frequency");

   rr = set_PWM_range(GPIO, 100);
   CHECK(3, 5, rr, 200, 0, "set PWM range");

   for (t=0; t<4; t++)
   {
      set_PWM_dutycycle(GPIO, dc[t]);
      time_sleep(1);
      t3_reset = 1;
      time_sleep(2);
      on = t3_on;
      off = t3_off;
      CHECK(3, 6+t, (1000.0*on)/(on+off), 10.0*dc[t], 1,
         "set PWM dutycycle");
   }

   set_PWM_dutycycle(GPIO, 0);

}

void t4()
{
   int h, e, f, n, s, b, l, seq_ok, toggle_ok;
   gpioReport_t r;
   char p[32];

   printf("Pipe notification tests.\n");

   set_PWM_frequency(GPIO, 0);
   set_PWM_dutycycle(GPIO, 0);
   set_PWM_range(GPIO, 100);

   h = notify_open();
   e = notify_begin(h, (1<<4));
   CHECK(4, 1, e, 0, 0, "notify open/begin");

   time_sleep(1);

   sprintf(p, "/dev/pigpio%d", h);

   f = open(p, O_RDONLY);

   set_PWM_dutycycle(GPIO, 50);
   time_sleep(4);
   set_PWM_dutycycle(GPIO, 0);

   e = notify_pause(h);
   CHECK(4, 2, e, 0, 0, "notify pause");

   e = notify_close(h);
   CHECK(4, 3, e, 0, 0, "notify close");

   n = 0;
   s = 0;
   l = 0;
   seq_ok = 1;
   toggle_ok = 1;

   while (1)
   {
      b = read(f, &r, 12);
      if (b == 12)
      {
         if (s != r.seqno) seq_ok = 0;

         if (n) if (l != (r.level&(1<<4))) toggle_ok = 0;

         if (r.level&(1<<4)) l = 0;
         else                l = (1<<4);
           
         s++;
         n++;

         // printf("%d %d %d %X\n", r.seqno, r.flags, r.tick, r.level);
      }
      else break;
   }
   close(f);
 
   CHECK(4, 4, seq_ok, 1, 0, "sequence numbers ok");

   CHECK(4, 5, toggle_ok, 1, 0, "gpio toggled ok");

   CHECK(4, 6, n, 80, 10, "number of notifications");
}

int t5_count = 0;

void t5cbf(unsigned gpio, unsigned level, uint32_t tick)
{
   t5_count++;
}

void t5()
{
   int BAUD=4800;

   char *TEXT=
"\n\
Now is the winter of our discontent\n\
Made glorious summer by this sun of York;\n\
And all the clouds that lour'd upon our house\n\
In the deep bosom of the ocean buried.\n\
Now are our brows bound with victorious wreaths;\n\
Our bruised arms hung up for monuments;\n\
Our stern alarums changed to merry meetings,\n\
Our dreadful marches to delightful measures.\n\
Grim-visaged war hath smooth'd his wrinkled front;\n\
And now, instead of mounting barded steeds\n\
To fright the souls of fearful adversaries,\n\
He capers nimbly in a lady's chamber\n\
To the lascivious pleasing of a lute.\n\
";

   gpioPulse_t wf[] =
   {
      {1<<GPIO, 0,  10000},
      {0, 1<<GPIO,  30000},
      {1<<GPIO, 0,  60000},
      {0, 1<<GPIO, 100000},
   };

   int e, oc, c;

   char text[2048];

   printf("Waveforms & serial read/write tests.\n");

   callback(GPIO, FALLING_EDGE, t5cbf);

   set_mode(GPIO, PI_OUTPUT);

   e = wave_clear();
   CHECK(5, 1, e, 0, 0, "callback, set mode, wave clear");

   e = wave_add_generic(4, wf);
   CHECK(5, 2, e, 4, 0, "pulse, wave add generic");

   e = wave_tx_repeat();
   CHECK(5, 3, e, 9, 0, "wave tx repeat");

   oc = t5_count;
   time_sleep(5);
   c = t5_count - oc;
   CHECK(5, 4, c, 50, 1, "callback");

   e = wave_tx_stop();
   CHECK(5, 5, e, 0, 0, "wave tx stop");

   e = serial_read_open(GPIO, BAUD);
   CHECK(5, 6, e, 0, 0, "serial read open");

   wave_clear();
   e = wave_add_serial(GPIO, BAUD, 5000000, strlen(TEXT), TEXT);
   CHECK(5, 7, e, 3405, 0, "wave clear, wave add serial");

   e = wave_tx_start();
   CHECK(5, 8, e, 6811, 0, "wave tx start");

   oc = t5_count;
   time_sleep(3);
   c = t5_count - oc;
   CHECK(5, 9, c, 0, 0, "callback");

   oc = t5_count;
   while (wave_tx_busy()) time_sleep(0.1);
   time_sleep(0.1);
   c = t5_count - oc;
   CHECK(5, 10, c, 1702, 0, "wave tx busy, callback");

   c = serial_read(GPIO, text, sizeof(text)-1);
   if (c > 0) text[c] = 0; /* null terminate string */
   CHECK(5, 11, strcmp(TEXT, text), 0, 0, "wave tx busy, serial read");

   e = serial_read_close(GPIO);
   CHECK(5, 12, e, 0, 0, "serial read close");

   c = wave_get_micros();
   CHECK(5, 13, c, 6158704, 0, "wave get micros");

   c = wave_get_high_micros();
   CHECK(5, 14, c, 6158704, 0, "wave get high micros");

   c = wave_get_max_micros();
   CHECK(5, 15, c, 1800000000, 0, "wave get max micros");

   c = wave_get_pulses();
   CHECK(5, 16, c, 3405, 0, "wave get pulses");

   c = wave_get_high_pulses();
   CHECK(5, 17, c, 3405, 0, "wave get high pulses");

   c = wave_get_max_pulses();
   CHECK(5, 18, c, 12000, 0, "wave get max pulses");

   c = wave_get_cbs();
   CHECK(5, 19, c, 6810, 0, "wave get cbs");

   c = wave_get_high_cbs();
   CHECK(5, 20, c, 6810, 0, "wave get high cbs");

   c = wave_get_max_cbs();
   CHECK(5, 21, c, 25016, 0, "wave get max cbs");
}

int t6_count=0;
int t6_on=0;
uint32_t t6_on_tick=0;

void t6cbf(unsigned gpio, unsigned level, uint32_t tick)
{
   if (level == 1)
   {
      t6_on_tick = tick;
      t6_count++;
   }
   else
   {
      if (t6_on_tick) t6_on += (tick - t6_on_tick);
   }
}

void t6()
{
   int tp, t, p;

   printf("Trigger tests.\n");

   gpio_write(GPIO, PI_LOW);

   tp = 0;

   callback(GPIO, EITHER_EDGE, t6cbf);

   time_sleep(0.2);

   for (t=0; t<10; t++)
   {
      time_sleep(0.1);
      p = 10 + (t*10);
      tp += p;
      gpio_trigger(4, p, 1);
   }

   time_sleep(0.5);

   CHECK(6, 1, t6_count, 10, 0, "gpio trigger count");

   CHECK(6, 2, t6_on, tp, 25, "gpio trigger pulse length");
}

int t7_count=0;

void t7cbf(unsigned gpio, unsigned level, uint32_t tick)
{
   if (level == PI_TIMEOUT) t7_count++;
}

void t7()
{
   int c, oc;

   printf("Watchdog tests.\n");

   /* type of edge shouldn't matter for watchdogs */
   callback(GPIO, FALLING_EDGE, t7cbf);

   set_watchdog(GPIO, 10); /* 10 ms, 100 per second */
   time_sleep(0.5);
   oc = t7_count;
   time_sleep(2);
   c = t7_count - oc;
   CHECK(7, 1, c, 200, 1, "set watchdog on count");

   set_watchdog(GPIO, 0); /* 0 switches watchdog off */
   time_sleep(0.5);
   oc = t7_count;
   time_sleep(2);
   c = t7_count - oc;
   CHECK(7, 2, c, 0, 1, "set watchdog off count");
}

void t8()
{
   int v, t, i;

   printf("Bank read/write tests.\n");

   gpio_write(GPIO, 0);
   v = read_bank_1() & (1<<GPIO);
   CHECK(8, 1, v, 0, 0, "read bank 1");

   gpio_write(GPIO, 1);
   v = read_bank_1() & (1<<GPIO);
   CHECK(8, 2, v, (1<<GPIO), 0, "read bank 1");

   clear_bank_1(1<<GPIO);
   v = gpio_read(GPIO);
   CHECK(8, 3, v, 0, 0, "clear bank 1");

   set_bank_1(1<<GPIO);
   v = gpio_read(GPIO);
   CHECK(8, 4, v, 1, 0, "set bank 1");

   t = 0;
   v = (1<<16);
   for (i=0; i<100; i++)
   {
      if (read_bank_2() & v) t++;
   };
   CHECK(8, 5, t, 60, 75, "read bank 2");

   v = clear_bank_2(0);
   CHECK(8, 6, v, 0, 0, "clear bank 2");

   v = clear_bank_2(0xffffff);
   CHECK(8, 7, v, PI_SOME_PERMITTED, 0, "clear bank 2");

   v = set_bank_2(0);
   CHECK(8, 8, v, 0, 0, "set bank 2");

   v = set_bank_2(0xffffff);
   CHECK(8, 9, v, PI_SOME_PERMITTED, 0, "set bank 2");
}

int t9_count = 0;

void t9cbf(unsigned gpio, unsigned level, uint32_t tick)
{
   t9_count++;
}

void t9()
{
   int s, oc, c, e;
   uint32_t p[10];

   printf("Script store/run/status/stop/delete tests.\n");

   gpio_write(GPIO, 0); /* need known state */

   /*
   100 loops per second
   p0 number of loops
   p1 GPIO
   */
   char *script="\
   ldap 0\
   ldva 0\
   label 0\
   w p1 1\
   milli 5\
   w p1 0\
   milli 5\
   dcrv 0\
   ldav 0\
   ldpa 9\
   jp 0";

   callback(GPIO, RISING_EDGE, t9cbf);

   s = store_script(script);
   oc = t9_count;
   p[0] = 99;
   p[1] = GPIO;
   run_script(s, 2, p);
   time_sleep(2);
   c = t9_count - oc;
   CHECK(9, 1, c, 100, 0, "store/run script");

   oc = t9_count;
   p[0] = 200;
   p[1] = GPIO;
   run_script(s, 2, p);
   while (1)
   {
      e = script_status(s, p);
      if (e != PI_SCRIPT_RUNNING) break;
      time_sleep(0.5);
   }
   c = t9_count - oc;
   time_sleep(0.1);
   CHECK(9, 2, c, 201, 0, "run script/script status");

   oc = t9_count;
   p[0] = 2000;
   p[1] = GPIO;
   run_script(s, 2, p);
   while (1)
   {
      e = script_status(s, p);
      if (e != PI_SCRIPT_RUNNING) break;
      if (p[9] < 1900) stop_script(s);
      time_sleep(0.1);
   }
   c = t9_count - oc;
   time_sleep(0.1);
   CHECK(9, 3, c, 110, 10, "run/stop script/script status");

   e = delete_script(s);
   CHECK(9, 4, e, 0, 0, "delete script");
}

int main(int argc, char *argv[])
{
   int t, status;
   void (*test[])(void) = {t0, t1, t2, t3, t4, t5, t6, t7, t8, t9};

   status = pigpio_start(0, 0);

   if (status < 0)
   {
      fprintf(stderr, "pigpio initialisation failed.\n");
      return 1;
   }

   printf("Connected to pigpio daemon.\n");

   for (t=0; t<10; t++) test[t]();

   pigpio_stop();

   return 0;
}

