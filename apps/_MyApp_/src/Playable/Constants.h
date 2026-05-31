#include "vector"
#include <vmath.h>

namespace TopdownShooter::Playable
{
	struct PlayerTextureConfig
	{
		const int DrawOrder = -1;
		const char *TexturePath = nullptr;
		const int ColCount = -1;
		const int RowCount = -1;
		const bool Flip = false;
	};
	const std::vector<PlayerTextureConfig> BACK_IDLE = {
	    {0, "./resources/texture/player/BACK_IDLE_H_0.png", 1, 1, false},
	    {1, "./resources/texture/player/BACK_IDLE_E_1.png", 1, 1, false},
	    {2, "./resources/texture/player/BACK_IDLE_B_2.png", 1, 1, false},
	    {3, "./resources/texture/player/BACK_IDLE_F_3.png", 1, 1, false},
	};

	const std::vector<PlayerTextureConfig> FRONT_IDLE = {
	    {0, "./resources/texture/player/FRONT_IDLE_E_0.png", 1, 1, false},
	    {1, "./resources/texture/player/FRONT_IDLE_H_1.png", 1, 1, false},
	    {2, "./resources/texture/player/FRONT_IDLE_B_2.png", 1, 1, false},
	    {3, "./resources/texture/player/FRONT_IDLE_F_3.png", 1, 1, false},
	};

	const std::vector<PlayerTextureConfig> LEFT_IDLE = {
	    {0, "./resources/texture/player/LEFT_IDLE_E_0.png", 1, 1, false},
	    {1, "./resources/texture/player/LEFT_IDLE_H_1.png", 1, 1, false},
	    {2, "./resources/texture/player/LEFT_IDLE_B_2.png", 1, 1, false},
	    {3, "./resources/texture/player/LEFT_IDLE_F_3.png", 1, 1, false},
	};

	const std::vector<PlayerTextureConfig> RIGHT_IDLE = {
	    {0, "./resources/texture/player/LEFT_IDLE_E_0.png", 1, 1, true},
	    {1, "./resources/texture/player/LEFT_IDLE_H_1.png", 1, 1, true},
	    {2, "./resources/texture/player/LEFT_IDLE_B_2.png", 1, 1, true},
	    {3, "./resources/texture/player/LEFT_IDLE_F_3.png", 1, 1, true},
	};

	const std::vector<PlayerTextureConfig> FRONT_MOVE = {
	    {0, "./resources/texture/player/FRONT_MOVE_E_0.png", 1, 1},
	    {1, "./resources/texture/player/FRONT_MOVE_H_1.png", 1, 1},
	    {2, "./resources/texture/player/FRONT_MOVE_B_2.png", 1, 2},
	    {3, "./resources/texture/player/FRONT_MOVE_F_3.png", 1, 1},
	};

	const std::vector<PlayerTextureConfig> BACK_MOVE = {
	    {0, "./resources/texture/player/BACK_MOVE_B_0.png", 1, 1},
	    {1, "./resources/texture/player/BACK_MOVE_H_1.png", 1, 2},
	    {2, "./resources/texture/player/BACK_MOVE_E_2.png", 1, 2},
	    {3, "./resources/texture/player/BACK_MOVE_F_3.png", 1, 1},
	};

	const std::vector<PlayerTextureConfig> LEFT_MOVE = {
	    {0, "./resources/texture/player/LEFT_MOVE_E_0.png", 1, 1},
	    {1, "./resources/texture/player/LEFT_MOVE_H_1.png", 1, 1},
	    {2, "./resources/texture/player/LEFT_MOVE_B_2.png", 1, 2},
	    {3, "./resources/texture/player/LEFT_MOVE_F_3.png", 1, 1},
	};

	const std::vector<PlayerTextureConfig> RIGHT_MOVE = {
	    {0, "./resources/texture/player/LEFT_MOVE_E_0.png", 1, 1, true},
	    {1, "./resources/texture/player/LEFT_MOVE_H_1.png", 1, 1, true},
	    {2, "./resources/texture/player/LEFT_MOVE_B_2.png", 1, 2, true},
	    {3, "./resources/texture/player/LEFT_MOVE_F_3.png", 1, 1, true},
	};
}; // namespace TopdownShooter::Playable
