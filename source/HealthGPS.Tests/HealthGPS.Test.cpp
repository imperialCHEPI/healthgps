#include "pch.h"
#include "CppUnitTest.h"
#include "..\HealthGPS.Console\include\main.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace HealthGPSTest
{
	TEST_CLASS(MainTest)
	{
	public:
		
		TEST_METHOD(LocalTimeNow)
		{
			Assert::AreEqual(1, 1);
			Assert::IsTrue(true);
			Assert::IsFalse(hgps::getTimeNowStr().empty());
		}

		/*
		TEST_METHOD(InvalidLocalTime)
		{
			Assert::IsTrue(hgps::getTimeNowStr().empty());
		}
		*/
	};
}
