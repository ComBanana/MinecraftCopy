#include "Player.h"

#include <cmath>
#include <iostream>

int main()
{
    World world(WorldType::Infinite);
    for (int x = 0; x < 5; ++x)
    {
        for (int z = 0; z < 5; ++z)
            world.setBlock({x, 0, z}, BlockId::Stone);
    }
    world.setBlock({2, 1, 2}, BlockId::Stone);
    world.setBlock({2, 2, 2}, BlockId::Stone);

    Player player;
    player.setPosition({0.5f, 5.0f, 2.5f});
    const PlayerInput noInput;
    for (int frame = 0; frame < 180; ++frame)
        player.updateMovement(1.0f / 60.0f, world, noInput);

    if (std::abs(player.getPosition().y - 2.8f) > 0.02f)
    {
        std::cerr << "Player did not land on the block floor.\n";
        return 1;
    }

    PlayerInput jumpInput;
    jumpInput.jump = true;
    player.updateMovement(1.0f / 60.0f, world, jumpInput);
    if (player.getPosition().y <= 2.9f)
    {
        std::cerr << "Player did not jump from the floor.\n";
        return 1;
    }

    player.setPosition({0.5f, 2.8f, 2.5f});
    PlayerInput moveRight;
    moveRight.right = true;
    for (int frame = 0; frame < 30; ++frame)
        player.updateMovement(0.05f, world, moveRight);
    if (player.getPosition().x > 1.71f || player.getPosition().x < 1.5f)
    {
        std::cerr << "Player did not stop at the wall collision.\n";
        return 1;
    }

    World finiteWorld(WorldType::Limited);
    Player edgePlayer;
    edgePlayer.setPosition({398.5f, 4.0f, 10.5f});
    for (int frame = 0; frame < 30; ++frame)
        edgePlayer.updateMovement(0.05f, finiteWorld, moveRight);
    if (edgePlayer.getPosition().x > 399.71f)
    {
        std::cerr << "Player crossed the finite world boundary.\n";
        return 1;
    }

    std::cout << "Player tests passed.\n";
    return 0;
}
