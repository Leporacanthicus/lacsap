#include <stdio.h>

extern void __PascalMain(void);

void __write_int(int v, int width)
{
    printf("%*d", width, v);
}

void __write_real(double v, int width, int precision)
{
    if (precision > 0)
    {
	printf("%*.*f", width, precision, v);
    }
    else
    {
	printf("%*E", width, v);
    }
}

void __write_char(char v, int width)
{
    if (width > 0)
    {
	printf("%*c", width, v);
    }
    else
    {
	printf("%c", v);
    }
}    

void __write_str(const char *v, int width)
{
    if (width > 0)
    {
	printf("%*s", width, v);
    }
    else
    {
	printf("%s", v);
    }
}

void __read_int(int *v)
{
    scanf("%d", v);
}

void __read_real(double *v)
{
    scanf("%lf", v);
}

void __read_nl()
{
    while(getchar() != '\n')
	;
}


void __write_nl(void)
{
    putchar('\n');
}


int main()
{
    __PascalMain();
}
