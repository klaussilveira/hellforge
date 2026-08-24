#include "RadiantTest.h"

#include "icommandsystem.h"
#include "imap.h"
#include "ipatch.h"
#include "iundo.h"

#include "algorithm/Scene.h"
#include "noise/Noise.h"

#include <algorithm>

namespace test
{

using TerrainGeneratorTest = RadiantTest;

void executeGenerateTerrain(int algorithm, int seed = 42, double frequency = 0.01,
	double amplitude = 64.0, int octaves = 4, double persistence = 0.5,
	double lacunarity = 2.0, int columns = 5, int rows = 5,
	double width = 512.0, double height = 512.0, double spawnX = 0.0,
	double spawnY = 0.0, double spawnZ = 0.0,
	const std::string& material = "textures/common/caulk")
{
	GlobalCommandSystem().executeCommand("GenerateTerrain",
		{ cmd::Argument(algorithm),
		  cmd::Argument(seed),
		  cmd::Argument(frequency),
		  cmd::Argument(amplitude),
		  cmd::Argument(octaves),
		  cmd::Argument(persistence),
		  cmd::Argument(lacunarity),
		  cmd::Argument(columns),
		  cmd::Argument(rows),
		  cmd::Argument(width),
		  cmd::Argument(height),
		  cmd::Argument(spawnX),
		  cmd::Argument(spawnY),
		  cmd::Argument(spawnZ),
		  cmd::Argument(material) });
}

TEST_F(TerrainGeneratorTest, BasicTerrainGeneration)
{
	const int columns = 5;
	const int rows = 5;
	const std::string material = "textures/common/caulk";

	executeGenerateTerrain(0, 42, 0.01, 64.0, 4, 0.5, 2.0,
		columns, rows, 512.0, 512.0, 0.0, 0.0, 0.0, material);

	auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

	// Find the generated patch
	auto patchNode = algorithm::findFirstPatchWithMaterial(worldspawn, material);
	ASSERT_TRUE(patchNode) << "Terrain patch should have been created";

	auto* patch = Node_getIPatch(patchNode);
	ASSERT_NE(patch, nullptr);

	// Check that the resulting patch was created
	EXPECT_EQ(patch->getWidth(), columns);
	EXPECT_EQ(patch->getHeight(), rows);
	EXPECT_EQ(patch->getShader(), material);
}

TEST_F(TerrainGeneratorTest, CanUndoTerrainGeneration)
{
	auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
	auto childCountBefore = algorithm::getChildCount(worldspawn);

	// Call terrain generator with default args
	executeGenerateTerrain(0);

	EXPECT_EQ(algorithm::getChildCount(worldspawn), childCountBefore + 1)
		<< "One patch should have been added";

	GlobalUndoSystem().undo();

	EXPECT_EQ(algorithm::getChildCount(worldspawn), childCountBefore)
		<< "Undo should remove the generated terrain patch";
}

TEST_F(TerrainGeneratorTest, AllPresetsProduceCentredVariation)
{
	for (int i = 0; i < noise::AlgorithmCount; ++i)
	{
		auto algorithm = static_cast<noise::Algorithm>(i);

		noise::NoiseParameters params;
		params.algorithm = algorithm;
		params.seed = 42;
		params.frequency = 0.02;
		params.amplitude = 1.0;

		noise::NoiseGenerator generator(params);

		double lowest = 0.0;
		double highest = 0.0;

		for (int x = 0; x < 150; ++x)
		{
			for (int y = 0; y < 150; ++y)
			{
				double value = generator.sample(x * 10.0, y * 10.0);
				lowest = std::min(lowest, value);
				highest = std::max(highest, value);
			}
		}

		const char* name = noise::getAlgorithmName(algorithm);

		EXPECT_LT(lowest, -0.2) << name << " should displace terrain downwards";
		EXPECT_GT(highest, 0.2) << name << " should displace terrain upwards";
		EXPECT_GT(lowest, -1.25) << name << " should stay within the amplitude range";
		EXPECT_LT(highest, 1.25) << name << " should stay within the amplitude range";
	}
}

TEST_F(TerrainGeneratorTest, OutOfRangeAlgorithmIsRejected)
{
	auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
	auto childCountBefore = algorithm::getChildCount(worldspawn);

	executeGenerateTerrain(noise::AlgorithmCount);

	EXPECT_EQ(algorithm::getChildCount(worldspawn), childCountBefore)
		<< "An out of range algorithm index should not generate a patch";

	executeGenerateTerrain(-1);

	EXPECT_EQ(algorithm::getChildCount(worldspawn), childCountBefore)
		<< "A negative algorithm index should not generate a patch";
}

TEST_F(TerrainGeneratorTest, AllPresetsShareEffectiveFeatureSize)
{
	for (int i = 0; i < noise::AlgorithmCount; ++i)
	{
		auto algorithm = static_cast<noise::Algorithm>(i);

		noise::NoiseParameters params;
		params.algorithm = algorithm;
		params.seed = 42;
		params.frequency = 0.01;
		params.amplitude = 1.0;

		noise::NoiseGenerator generator(params);

		long crossings = 0;
		double scanned = 0.0;

		for (int line = 0; line < 12; ++line)
		{
			double y = line * 613.0;
			double previous = generator.sample(0.0, y);

			for (int step = 1; step < 8000; ++step)
			{
				double value = generator.sample(step * 0.5, y);

				if ((value < 0.0) != (previous < 0.0))
				{
					++crossings;
				}

				previous = value;
			}

			scanned += 4000.0;
		}

		ASSERT_GT(crossings, 0) << noise::getAlgorithmName(algorithm) << " produced a flat field";

		double featureSize = 2.0 * scanned / crossings;
		const char* name = noise::getAlgorithmName(algorithm);

		EXPECT_GT(featureSize, 160.0) << name
			<< " is too fine at frequency 0.01 and will alias on a coarse patch";
		EXPECT_LT(featureSize, 250.0) << name
			<< " is too coarse at frequency 0.01 to read as terrain";
	}
}

} // namespace test
