#include "demo.h"

#include "rendering.h"
#include "utility.h"

#include <filesystem>
#include <iostream>
#include <vector>

using namespace Cubiquity;
namespace fs = std::filesystem;

// See https ://stackoverflow.com/a/11142540
void findVolumes(const fs::path& root, std::vector<fs::path>& paths)
{
	std::cout << "\t Searching " << root << std::endl;
	if (!fs::exists(root) || !fs::is_directory(root)) return;

	fs::recursive_directory_iterator it(root);
	fs::recursive_directory_iterator endit;

	while (it != endit)
	{
		if (fs::is_regular_file(*it) && it->path().extension() == ".vol")
		{
			paths.push_back(it->path());
		}
		++it;
	}
}

Demo::Demo(int argc, char** argv, WindowType windowType)
	: Window(windowType)
	, mArgs(argc, argv)
{
}

void Demo::onInitialise()
{
	std::string path;
	if(mArgs.positional().size() >= 1)
	{
		path = mArgs.positional()[0];
	}
	else
	{
		std::cout << "No volume specified - searching filesystem..." << std::endl;
		std::vector<fs::path> paths;
		findVolumes("data", paths);
		findVolumes("../data", paths);

		if (paths.size() > 0)
		{
			std::cout << std::endl << "Found the following volumes:" << std::endl;
			for (uint ct = 0; ct < paths.size(); ct++)
			{
				std::cout << "\t" << ct + 1 << ". " << paths[ct] << std::endl;
			}
			int pathIndex;
			do
			{
				std::cout << std::endl << "Choose a volume (enter a number 1 - " << paths.size() << "): ";
				std::string input;
				std::cin >> input;
				try
				{
					pathIndex = std::stoi(input);
				}
				catch (std::exception& e)
				{
					continue;
				}
			} while (pathIndex < 1 || pathIndex > paths.size());
			path = paths[pathIndex - 1].string();
		}
		else
		{
			std::cout << "No volumes found!" << std::endl;
		}
	}

	if (!path.empty())
	{
		std::cout << "Opening volume \'" << path << "\'... ";
		if (!mVolume.load(path))
		{
			std::cout << " failed to open volume!" << std::endl;
			exit(EXIT_FAILURE);
		}
		std::cout << " done" << std::endl;

		mVolume.setTrackEdits(true);

		//Box3i bounds = computeBounds(mVolume, [](MaterialId matId) { return matId != 0; });
		auto result = estimateBounds(mVolume);
		MaterialId outsideMaterialId = result.first;
		Box3i bounds = result.second;

		if (outsideMaterialId == 0) // Solid object, point camera at centre and move it back
		{
			Vector3f centre = bounds.centre();
			float halfDiagonal = length((bounds.upper() - bounds.lower())) * 0.5f;

			// Centred along x, then back and up a bit
			mCamera.position = Vector3f(centre.x(), centre.y() - halfDiagonal, centre.z() + halfDiagonal);

			// Look down 45 degrees
			mCamera.pitch = -(Pi / 4.0f);
			mCamera.yaw = 0.0f;
		}
		else // Hollow object, place camera at centre.
		{
			Vector3f centre = bounds.centre();
			centre += Vector3f(0.1, 0.1, 0.1); // Hack to help not be on a certain boundary which triggers assert in debug mode.
			mCamera.position = Vector3f(centre.x(), centre.y(), centre.z());

			// Look straight ahead
			mCamera.pitch = 0.0f;
			mCamera.yaw = 0.0f;
		}
	}
}

void Demo::onUpdate(float deltaTime)
{	
	// Move forward
	if (keyState(SDL_SCANCODE_W) == KeyState::Down)
	{
		mCamera.position += mCamera.forward() * deltaTime * CameraMoveSpeed;
		onCameraModified();
	}
	// Move backward
	if (keyState(SDL_SCANCODE_S) == KeyState::Down)
	{
		mCamera.position -= mCamera.forward() * deltaTime * CameraMoveSpeed;
		onCameraModified();
	}
	// Strafe right
	if (keyState(SDL_SCANCODE_D) == KeyState::Down)
	{
		mCamera.position += mCamera.right() * deltaTime * CameraMoveSpeed;
		onCameraModified();
	}
	// Strafe left
	if (keyState(SDL_SCANCODE_A) == KeyState::Down)
	{
		mCamera.position -= mCamera.right() * deltaTime * CameraMoveSpeed;
		onCameraModified();
	}
}

void Demo::onKeyUp(const SDL_KeyboardEvent& event)
{
	if (event.keysym.sym == SDLK_ESCAPE)
	{
		close();
	}

	if ((event.keysym.mod & KMOD_CTRL) && event.keysym.sym == SDLK_z)
	{
		event.keysym.mod& KMOD_SHIFT ? mVolume.redo() : mVolume.undo();
		onVolumeModified();
	}
}

void Demo::onMouseMotion(const SDL_MouseMotionEvent& event)
{
	if (mouseButtonState(SDL_BUTTON_RIGHT) == MouseButtonState::Down)
	{
		// Compute new orientation. Window origin is top-left (not bottom-
		// left) So we subtract the relative y motion instead of adding it.
		mCamera.yaw += CameraTurnSpeed * event.xrel;
		mCamera.pitch -= CameraTurnSpeed * event.yrel;
		onCameraModified();
	}
}

void Demo::onMouseButtonDown(const SDL_MouseButtonEvent& event)
{
	if (event.button == SDL_BUTTON(SDL_BUTTON_LEFT))
	//if (mouseButtonState(SDL_BUTTON_LEFT) == MouseButtonState::Down)
	{
		//std::cout << "x = " << event.x << ", y = " << event.y << std::endl;

		Ray3d ray = mCamera.rayFromViewportPos(event.x, event.y, width(), height());

		RayVolumeIntersection intersection = ray_parameter(mVolume, ray);
		if (intersection)
		{
			SphereBrush brush(Vector3f(intersection.position.x(), intersection.position.y(), intersection.position.z()), 30);
			mVolume.fillBrush(brush, 0);

			onVolumeModified();
		}
	}

	if(event.button == SDL_BUTTON(SDL_BUTTON_RIGHT))
	//if (mouseButtonState(SDL_BUTTON_RIGHT) == MouseButtonState::Down)
	{
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
}

void Demo::onMouseButtonUp(const SDL_MouseButtonEvent& event)
{
	if (event.button == SDL_BUTTON(SDL_BUTTON_RIGHT))
	//if (mouseButtonState(SDL_BUTTON_RIGHT) == MouseButtonState::Down)
	{
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
}
