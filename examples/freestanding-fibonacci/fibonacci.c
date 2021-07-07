void print_str(char *str_addr, unsigned int len);

// calculate fibonacci number (n_0 := 0)
unsigned int fibonacci(unsigned int n) {
    // f_0 = f_1 = 1
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 2) + fibonacci(n - 1);
}

void itoa(char *str_addr, unsigned int num, unsigned int num_digits) {
    for (int j = num_digits - 1; j >= 0; j--) {
        str_addr[j] = num % 10 + '0';
        num /= 10;
    }
}

int main() {
    unsigned int n = 20;
    unsigned int fib = fibonacci(n);

    unsigned int num_digits = 0;
    for (unsigned int number = fib; number > 0; number /= 10) {
        num_digits++;
    }

    char out_str[num_digits + 1];
    itoa(out_str, fib, num_digits);
    out_str[num_digits] = '\n';

    // this should print 6765
    print_str(out_str, num_digits + 1);
    return 0;
}
