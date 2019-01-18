#include <stdio.h>
#include <string.h>


int sum47(int x) {

  int sum = 1;

  int y = 2;
  int holder = 0;
  int add = 0;
  int minus = 0;

  while (y < 7) {

    if (x % 3 == 1) {

      if (add == 0 && minus == 0) {
        add = 1;
        holder = holder * 10 + y;

      } else if (add == 1 && minus == 0) {
        sum = sum + holder;
        holder = y;
        add = 1;

      } else if (add == 0 && minus == 1) {
        sum = sum - holder;
        minus = 0;
        holder = y;
        add = 1;

      }

    } else if (x % 3 == 2) {

      if (minus == 0 && add == 0) {
        minus = 1;
        holder = holder * 10 + y;

      } else if (minus == 1 && add == 0) {
        sum = sum - holder;
        holder = y;
        minus = 1;

      } else if (minus == 0 && add == 1) {
        sum = sum + holder;
        minus = 1;
        holder = y;
        add = 0;
      }

    } else if (x % 3 == 0) {

      if ((add == 1 && minus == 0) || (minus == 1 && add == 0)) {
        holder = holder * 10 + y;
      } else {
        sum = sum * 10 + y;
      }
    }

    x = x / 3;
    y = y + 1;

  }

  if (add == 1) {
    sum = sum + holder;
  }

  if (minus == 1) {
    sum = sum - holder;
  }

  return sum;
}



  int main() {
    // x, y and z will be used as counters
    int x = 0;
    int y = 2;
    int z = x;

    char equation[13] = "";
    char num[3] = "";
    char result[3000] = "";

    while (x < 243) {

      // check if the equation equals 47
      int check47 = sum47(x);
      if (check47 == 47) {

        strcpy(equation, "1");
        y = 2;
        z = x;

        while (y < 7) {

          if (z % 3 == 1) {
            strcat(equation, "+");
          }

          if (z % 3 == 2) {
            strcat(equation, "-");
          }

          sprintf(num, "%d", y);

          strcat(equation, num);

          strcpy(num, "");

          z = z / 3;
          y = y + 1;

        }
        strcat(result, equation);
        strcat(result, "\n");
      }

      // add to counter
      x = x + 1;
    }

    int wordlength = strlen(result);
    result[wordlength - 1] = 0;

    printf("%s", result);

    return 0;
  }

