/* Compile-only baseline: no peripheral or clock configuration yet. */
int main(void)
{
    for (;;) {
        __asm volatile ("nop");
    }
}
