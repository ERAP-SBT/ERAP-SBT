void print_str(char *str_addr, unsigned int len);

void itoa(char *str_addr, unsigned int num, unsigned int num_digits) {
    for (int j = num_digits - 1; j >= 0; j--) {
        str_addr[j] = num % 10 + '0';
        num /= 10;
    }
}

void print_integer(unsigned int integer) {
    unsigned int num_digits = 0;
    for (unsigned int number = integer; number > 0; number /= 10) {
        num_digits++;
    }

    char out_str[num_digits + 1];
    itoa(out_str, integer, num_digits);
    out_str[num_digits] = '\n';

    // this should print 6765
    print_str(out_str, num_digits + 1);
}

int main() {
    float a = 1.0;
    float b = 2.0;
    float sum = a + b;

    int iSum = (int) sum;
    print_integer(iSum);
    return 0;
}