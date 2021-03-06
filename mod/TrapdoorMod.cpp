//
// Created by xhy on 2020/12/23.
//

#include "TrapdoorMod.h"
#include "commands/Command.h"
#include "tick/GameTick.h"
#include "function/BackupHelper.h"
#include "os/process_stat.h"
#include "eval/Eval.h"
#include "trapdoor.h"
#include "VanillaBlockType.h"
#include "test/TrapdoorTest.h"
#include "lib/Remotery.h"

namespace mod {

    void TrapdoorMod::heavyTick() {
        rmt_ScopedCPUSample(MOD_TICK, 0);
        this->villageHelper.tick();
        this->hsaManager.tick();
        this->spawnHelper.tick();
        this->playerFunctions.tick();
        this->slimeChunkHelper.tick();
        this->simpleLitematica.tick();
        this->fakePlayerClient->tick();
    }

    void TrapdoorMod::lightTick() {
        this->hopperChannelManager.tick();
        //   this->spawnAnalyzer.tick();
    }

    //这个函数会在初始化Level对象后执行
    void TrapdoorMod::initialize() {
        BDSMod::initialize();
        this->commandManager.setCommandConfig(
                this->configManager.getCommandsConfig());
        mod::initBackup();
        this->villageHelper.setConfig(this->configManager.getVillageConfig());
        get_cpu_usage();
        this->initFunctionEnable();
        initBlockMap();
        //初始化假人客户端的线程池
        this->fakePlayerClient = new FakePlayerClient(this->getThreadPool());
        this->fakePlayerClient->registerFakePlayerCommand(commandManager);
        L_INFO("==== trapdoor init finish  ====\nServer Start");
    }

    void TrapdoorMod::registerCommands() {
        using namespace trapdoor;
        BDSMod::registerCommands();
        tick::registerTickCommand(this->commandManager);
        tick::registerProfileCommand(this->commandManager);
        mod::test::registerTestCommand(this->commandManager);
        mod::registerBackupCommand(this->commandManager);
        this->hsaManager.registerCommand(this->commandManager);
        this->simpleBuilder.registerDrawCommand(this->commandManager);
        // this->simpleLitematica.registerCommand(this->commandManager);
        this->villageHelper.registerCommand(this->commandManager);
        this->hopperChannelManager.registerCommand(this->commandManager);
        this->registerDevCommand();
        //功能开关命令
        commandManager.registerCmd("func", "command.func.desc")
                ->then(
                        ARG("hopper", "command.func.hopper.desc", BOOL,
                            {
                                this->hopperChannelManager.setAble(holder->getBool());
                                info(player, LANG("command.func.hopper.set"), holder->getBool());
                            }))
                ->then(ARG("spawn", "command.func.spawn.desc", BOOL,
                           {
                               this->spawnHelper.setAble(holder->getBool());
                               info(player, LANG("command.func.spawn.set"),
                                    holder->getBool());
                           }))
                ->then(ARG("rotate", "command.func.rotate.desc", BOOL,
                           {
                               this->rotationHelper.setAble(holder->getBool());
                               info(player, LANG("command.func.rotate.set"),
                                    holder->getBool());
                           }))
                ->then(ARG("draw", "command.func.draw.desc", BOOL,
                           {
                               this->simpleBuilder.setAble(holder->getBool());
                               info(player, LANG("command.func.draw.set"), holder->getBool());
                           }))
//                ->then(ARG(
//                               "stat", "command.func.stat.desc", BOOL,
//                               {
//                                   this->playerStatisticManager.setAble(holder->getBool());
//                                   info(player, LANG("command.func.stat.set"), holder->getBool());
//                               }))
                ->then(ARG("expl", "command.func.expl.desc", BOOL,
                           {
                               this->singleFunctions.preventExplosion = holder->getBool();
                               info(player, LANG("command.func.expl.set"), holder->getBool());
                           }))
                ->then(ARG("ncud", "command.func.ncud.desc", BOOL, {
                    this->singleFunctions.preventNCUpdate = holder->getBool();
                    info(player, LANG("command.func.ncud.set"), holder->getBool());
                }));

//史莱姆显示
        this->slimeChunkHelper.registerCommand(this->commandManager);
//漏斗计数器

//便捷模式切换
        commandManager.registerCmd("o", "command.o.desc")
                ->EXE({
                          player->setGameMode(4);
                          broadcastMsg(LANG("command.o.set"),
                                       player->getNameTag().c_str());
                      });

        commandManager.registerCmd("s", "command.s.desc")
                ->EXE({
                          player->setGameMode(0);
                          broadcastMsg(LANG("command.s.set"), player->getNameTag().c_str());
                      });

        commandManager.registerCmd("c", "command.c.desc")
                ->EXE({
                          player->setGameMode(1);
                          broadcastMsg(LANG("command.c.set"), player->getNameTag().c_str());
                      });


        commandManager.registerCmd("td?", "command.td?.desc")->EXE({
                                                                       this->commandManager.printfHelpInfo(player);
                                                                   });
        commandManager.registerCmd("self", "command.self.desc")
                ->
                        then(ARG(
                                     "chunk", "command.self.chunk.desc", BOOL,
                                     {
                                         if (!configManager.getSelfEnableConfig().enableChunkShow) {
                                             error(player, LANG("command.error.config"));
                                             return;
                                         }
                                         this->playerFunctions.setShowChunkAble(player->getNameTag(),
                                                                                holder->getBool());
                                         info(player, LANG("command.self.chunk.set"), holder->getBool());
                                     }))
                ->then(ARG(
                               "me", "command.self.me.desc", BOOL,
                               {
                                   if (!configManager.getSelfEnableConfig()
                                           .enableDistanceMeasure) {
                                       error(player, LANG("command.error.config"));
                                       return;
                                   }
                                   this->playerFunctions.getMeasureData(player->getNameTag())
                                           .enableMeasure = holder->getBool();
                                   info(player, LANG("command.self.me.set"), holder->getBool());
                               }))->
                        then(ARG("rs", "command.self.rs.desc", BOOL,
                                 {
                                     if (!configManager.getSelfEnableConfig()
                                             .enableRedstoneStick) {
                                         error(player, LANG("command.error.config"));
                                         return;
                                     }
                                     this->playerFunctions.setRedstoneHelperAble(
                                             player->getNameTag(), holder->getBool());
                                     info(player, LANG("command.self.rs.set"),
                                          holder->getBool());
                                 }))
                ->EXE({ PlayerFunction::printDebugInfo(player); });

        commandManager.registerCmd("here", "command.here.desc")->EXE({ PlayerFunction::broadcastSimpleInfo(player); });
        commandManager.registerCmd("l", "command.l.desc")->EXE({ PlayerFunction::listAllPlayers(player); });
        commandManager.registerCmd("os", "command.os.desc")->EXE({ TrapdoorMod::printOSInfo(player); });
        commandManager.registerCmd("cl", "command.cl.desc", Any, ArgType::STR)->EXE(
                { mod::eval(player, holder->getString()); });
    }


    void TrapdoorMod::printCopyRightInfo() {
        const char *banner =
                "\n"
                "  _______                  _                   \n"
                " |__   __|                | |                  \n"
                "    | |_ __ __ _ _ __   __| | ___   ___  _ __  \n"
                "    | | '__/ _` | '_ \\ / _` |/ _ \\ / _ \\| '__| \n"
                "    | | | | (_| | |_) | (_| | (_) | (_) | |    \n"
                "    |_|_|  \\__,_| .__/ \\__,_|\\___/ \\___/|_|    \n"
                "                | |                            \n"
                "                |_|                            ";
        printf(
                "%s\n  "
                "\ngithub:https://github.com/hhhxiao/TrapDoor\nLicense: GPL\n",
                banner);
        printf(
                "build time:     %s      "
                "%s\n-----------------------------------------------\n",
                __DATE__, __TIME__);
        fflush(stdout);
    }


    void TrapdoorMod::useOnHook(Actor *player,
                                const std::string &itemName,
                                BlockPos
                                &pos,
                                unsigned int facing,
                                const Vec3 &v
    ) {
//     L_INFO("%.2f %.2f %.2f", v.x,v.y,v.z , itemName.c_str());
//取消注释这一行可以看到右击地面的是什么东西
        if (itemName == "Bone" && this->spawnHelper.isEnable()) {
            spawnHelper.updateVerticalSpawnPositions(pos, player);
        } else if (itemName == "Gunpowder") {
            this->spawnHelper.printSpawnProbability(player, pos, 0);
        } else if (itemName == "Leather") {
            this->spawnHelper.printSpawnProbability(player, pos, 15);
        } else if (itemName == "Cactus") {
            this->hopperChannelManager.quickPrintData(player, pos);
            this->rotationHelper.rotate(pos, player->getBlockSource());
        } else if (itemName == "Wooden Sword") {
            this->playerFunctions.getMeasureData(player->getNameTag()).setPosition1(pos, player);
//  this->simpleLitematica.getSelectRegion().setPos1(pos, player);
        } else if (itemName == "Stone Sword") {
            this->playerFunctions.getMeasureData(player->getNameTag()).setPosition2(pos, player);
// this->simpleLitematica.getSelectRegion().setPos2(pos, player);
        } else if (itemName == "Stick") {
            this->playerFunctions.printRedstoneInfo(player, pos);
            auto *block = player->getBlockSource()->getBlock(pos);
            printf("id:%d name:%s variant:%d\n", block->getLegacy()->getBlockID(), block->getName().c_str(),
                   block->getVariant());
            fflush(stdout);
        }
    }

    CommandPermissionLevel TrapdoorMod::resetVanillaCommandLevel(
            const std::string &name,
            CommandPermissionLevel oldLevel) {
        auto lowLevelConfig = this->configManager.getLowLevelCommands();
        if (lowLevelConfig.find(name) != lowLevelConfig.end()) {
            L_DEBUG("set command %s level to gameMaster", name.c_str());
            return CommandPermissionLevel::GameMasters;
        } else {
            return oldLevel;
        }
    }


    void TrapdoorMod::printOSInfo(trapdoor::Actor *player) {
        int CPUUsage = get_cpu_usage();
        uint64_t memory, virtualMemory, ioRead, ioWrite;
        get_memory_usage(&memory, &virtualMemory);
        get_io_bytes(&ioRead, &ioWrite);
        std::string stringBuilder;
        stringBuilder += trapdoor::format(
                "CPU " C_INT "%%%%"
                "Mem: " C_INT " MB VMem; " C_INT" MB\n"
                "Read/Write" C_INT "KB / " C_INT  " KB",
                CPUUsage, memory >> 20u, virtualMemory >> 20u, ioRead >> 10u, ioWrite >> 10u
        );
        trapdoor::info(player, stringBuilder);
    }

/*
 * 实体攻击接口
@ return 返回false会阻止掉血,返回true会正常掉血
 @ entity1 进行攻击的实体
 @entity2 被攻击的实体
*/
    bool TrapdoorMod::attackEntityHook(Actor *entity1, Actor *entity2) {
        if (entity1->getActorId() != "player")
            return true;  //非玩家攻击直接忽略
        //开了居民村庄中心显示
        if (villageHelper.getShowDwellerStatus() &&
            (entity2->getActorId() == "iron_golem" ||
             entity2->getActorId() == "villager_v2")) {
            villageHelper.printDwellerInfo(entity1, entity2);
            return false;
        } else {
            return true;
        }
    }

    void TrapdoorMod::initFunctionEnable() {
        auto functionCfg = this->configManager.getFunctionConfig();
        this->spawnHelper.setAble(functionCfg.spawnHelper);
        this->rotationHelper.setAble(functionCfg.cactusRotation);
        this->simpleBuilder.setAble(functionCfg.simpleDraw);
        this->hopperChannelManager.setAble(functionCfg.hopperCounter);
    }


    void TrapdoorMod::registerDevCommand() {
        this->commandManager.registerCmd("dev", "develop")
                ->then(ARG("level_test", "test1", NONE, {
                    player->getLevel()->forEachPlayer([&](trapdoor::Actor *p) {
                        printf("%s\n", p->getNameTag().c_str());
                    });
                }));
    }
}  // namespace mod