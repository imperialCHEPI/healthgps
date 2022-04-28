#include "pch.h"
#include "HealthGPS.Core/math_util.h"

#include <limits>

TEST(TestCore_MathHelper, RadixValue)
{
	using namespace hgps::core;

	auto radix = MathHelper::radix();
	ASSERT_LT(0, radix);
	ASSERT_EQ(2, radix);
}

TEST(TestCore_MathHelper, MachinePrecision)
{
	using namespace hgps::core;

	auto precision = MathHelper::machine_precision();
	ASSERT_LT(0.0, precision);
}

TEST(TestCore_MathHelper, NumericalPrecision)
{
	using namespace hgps::core;

	auto precision = MathHelper::default_numerical_precision();
	ASSERT_LT(0.0, precision);
}

TEST(TestCore_MathHelper, EqualsDefaultPrecision)
{
	using namespace hgps::core;
	double a = (0.3 * 3.0) + 0.1;
	double b = 1.0;
	ASSERT_TRUE(MathHelper::equal(a, b));
}

TEST(TestCore_MathHelper, EqualsEpsilonPrecision)
{
	using namespace hgps::core;
	double a = 1;
	double b = 1 + std::numeric_limits<double>::epsilon();
	ASSERT_TRUE(MathHelper::equal(a, b));
}

TEST(TestCore_MathHelper, EqualsCustomPrecision)
{
	using namespace hgps::core;
	double a = (0.3 * 3.0) + 0.1;
	double b = 1.0;
	double tol = 1e-10;
	ASSERT_TRUE(MathHelper::equal(a, b, tol));
}

TEST(TestCore_MathHelper, EqualsZeroPrecision)
{
	using namespace hgps::core;
	double x = 0.0;
	double y = 1e-15;
	double tol = 1e-10;
	ASSERT_TRUE(MathHelper::equal(x, y, tol));
}
