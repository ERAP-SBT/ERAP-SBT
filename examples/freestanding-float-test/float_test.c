void print_str(char *str_addr, unsigned int len);

void itoa(char *str_addr, unsigned int num, unsigned int num_digits) {
    for (int j = num_digits - 1; j >= 0; j--) {
        str_addr[j] = num % 10 + '0';
        num /= 10;
    }
}

void print_integer(unsigned long long integer) {
    unsigned long long num_digits = 0;
    for (unsigned long long number = integer; number > 0; number /= 10) {
        num_digits++;
    }

    char out_str[num_digits + 1];
    itoa(out_str, integer, num_digits);
    out_str[num_digits] = '\n';

    print_str(out_str, num_digits + 1);
}

// exponentiation with a natural number as exponent
double own_pow(double base, unsigned long long exponent) {
    double result = 1;
    for (unsigned long long i = 0; i < exponent; i++) {
        result *= base;
    }

    return result;
}

int main() {
    // calcualte the sum of the little one (German: "Das kleine Einmaleins")
    double sum = 0;
    for (unsigned i = 1; i <= 10; i++) {
        for (unsigned j = 1; j <= 10; j++) {
            sum += i * j;
        }
    }

    // result should be: 3025
    unsigned long long i_sum = (unsigned long long)sum;
    print_integer(i_sum);

    double bases[] = {1.5, 1.7, 2.0, 2.5, 3.0};
    for (unsigned i = 0; i < 5; i++) {
        for (unsigned j = 0; j <= 20; j++) {
            double result = own_pow(bases[i], j);
            print_integer((unsigned long long)result);
        }

        print_str("\n", 2);
    }
}
