#include <string.h>
#include <stdio.h>

int main()
{
    char str[256] = "0123?hahahahaha";
    char *ret;
    ret = strchr(str, '?');
    printf("%p  %c  %s\n", ret, *(ret + 1), str);
}