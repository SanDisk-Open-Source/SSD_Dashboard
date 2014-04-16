#include <stdio.h>

int main()
{
	int ret, err = 0, e1, e2;
	double d1, d2;
	const char j1[] = "3.0E+10", j2[] = "12E-01-0.1E-2";
	const char a1[] = "3.0", a2[] = "-12,1000";

	/* XXX: check sscanf returned values too */

	/* double tests */
	ret = sscanf(j1, "%11lf", &d1);
	if (ret != 1) {
		printf("Error wrong sscanf double return %d.\n", ret);
		err++;
	}
	ret = sscanf(j2, "%11lf%11lf", &d1, &d2);
	if (ret != 2) {
		printf("Error wrong sscanf double return %d.\n", ret);
		err++;
	}

	/* int tests */
	ret = sscanf(a1, "%1d", &e1);
	if (ret != 1) {
		printf("Error wrong sscanf int return %d.\n", ret);
		err++;
	}
	ret = sscanf(a2, "%1d%2d", &e1, &e2);
	if (ret != 2) {
		printf("Error wrong sscanf int return %d.\n", ret);
		err++;
	}

	if (err)
		return err;
	return 0;
}
