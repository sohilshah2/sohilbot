#! /usr/bin/python3

import argparse
import chess
import chess.pgn
import chess.engine
import subprocess
import os.path
import random
import math
import scipy
import multiprocessing as mp

# Declared a draw after 300 halfmoves
MAX_MOVES = 300

parser = argparse.ArgumentParser(
                prog='run_tournament.py',
                description='Automates tournament between UCI engines.')
parser.add_argument('-t', '--threads', default=2)
parser.add_argument('-g', '--games', default=100) 
parser.add_argument('-m', '--movetime', default=100)
parser.add_argument('-p', '--positions', default="positions")
parser.add_argument('-f', '--force_engine1_white', action="store_true")
parser.add_argument('engine1')
parser.add_argument('engine2')

args = parser.parse_args()

def phiInv(p):
    return math.sqrt(2) * scipy.special.erfinv(2*p-1)

def eloDiff(p):
    return -400 * math.log((1/p) - 1) / math.log(10)

def calculateEloDifference(result):
    wins = result[0]
    losses = result[2]
    draws = result[1]

    score = wins + draws/2.0
    total = wins + draws + losses
    if (total == 0): 
        return ""

    p = float(score) / float(total)
    win_p = float(wins) / float(total)
    loss_p = float(losses) / float(total)
    draw_p = float(draws) / float(total)

    if (p == 0 or p == 1):
        return "Cannot calculate Elo diff - inf"

    diff = eloDiff(p)

    wins_dev = win_p * (1 - p) ** 2
    draws_dev = draw_p * (0.5 - p) ** 2
    losses_dev = loss_p * (0 - p) ** 2
    std_deviation = math.sqrt(wins_dev + draws_dev + losses_dev) / math.sqrt(total)

    confidence_p = 0.95
    min_confidence_p = (1 - confidence_p) / 2
    max_confidence_p = 1 - min_confidence_p
    dev_min = p + phiInv(min_confidence_p) * std_deviation
    dev_max = p + phiInv(max_confidence_p) * std_deviation

    if (dev_min <= 0 or dev_min >= 1 or dev_max <= 0 or dev_max >= 1):
        difference = 0
    else:
        difference = eloDiff(dev_max) - eloDiff(dev_min)

    return "Elo difference: " + str(round(diff, 1)) + " +/- " + str(round(difference / 2, 1)) + " [" + str(confidence_p * 100) + "% conf]"

def runGame(p1, p2, pos, game):
    global args

    board = chess.Board(fen=pos)

    if (args.force_engine1_white):
        engine1_color = chess.WHITE
    else:
        engine1_color = chess.WHITE if (random.randint(0,1) == 1) else chess.BLACK
    numMoves = 0

    rootNode = chess.pgn.Game()
    rootNode.headers["White"] = args.engine1 if (engine1_color == chess.WHITE) else args.engine2
    rootNode.headers["Black"] = args.engine2 if (engine1_color == chess.WHITE) else args.engine1

    rootNode.setup(board=board)
    node = rootNode
    while (not board.is_game_over(claim_draw=True)) and (numMoves < MAX_MOVES):
        numMoves += 1
        if (board.turn == engine1_color):
            result = p1.play(board, chess.engine.Limit(time=int(args.movetime)/1000.0))
        else:
            result = p2.play(board, chess.engine.Limit(time=int(args.movetime)/1000.0))
        board.push(result.move)
        node = node.add_variation(result.move)

    # [wins, draw, loss], perspective of engine1
    score = [0, 0, 0]

    if (board.is_checkmate()):
        if (board.turn == engine1_color):
            score[2] = 1
        else:
            score[0] = 1
    else:
        score[1] = 1

    dir = args.engine1 + "_" + args.engine2 + "_games"
    if not os.path.exists(dir):
        os.mkdir(dir)
    logfile = open(dir + "/log_game_" + str(game) + ".log", "w")
    print(rootNode, file=logfile, end="\n\n")

    return score

def runMatch(q, t, games):
    global args
    posFile = open(args.positions)
    positions = []
    for line in posFile:
        positions.append(line.strip())

    p1 = chess.engine.SimpleEngine.popen_uci("./"+args.engine1)
    p2 = chess.engine.SimpleEngine.popen_uci("./"+args.engine2)

    for game in range(games):
        rand = random.randint(0, len(positions)-1)
        pos = positions[rand]
        result = runGame(p1, p2, pos, t * int(args.threads) + game)
        q.put(result)

    p1.quit()
    p2.quit()

    q.close()

def main():
    global args

    if (int(args.movetime) < 20):
        print("Error! Movetime must be >=20ms")
        exit(1)
    
    if (not os.path.isfile(args.engine1)):
        print("Error! Could not find engine 1: " + args.engine1)
        exit(1)
    if (not os.path.isfile(args.engine2)):
        print("Error! Could not find engine 2: " + args.engine2)
        exit(1)
    
    q = mp.Queue(int(args.games))
    games_per_thread = int(args.games) / int(args.threads)
    p = []
    for t in range(int(args.threads)):
        p.append(mp.Process(target=runMatch, args=(q,t,int(games_per_thread))))
        p[t].start()

    results = [0, 0, 0]
    print("\n\n")
    for t in range(int(args.threads)):
        while (p[t].exitcode == None):
            p[t].join(timeout=1)
            while (not q.empty()):
                r = q.get()
                results[0] += r[0]
                results[1] += r[1]
                results[2] += r[2]
            print("\033[A\033[A\033[A\r", end="")
            print("Current score:  [" + args.engine1 + " wins:  " + str(results[0]) 
                  + " | draws:  " + str(results[1]) + " | " + args.engine2 + " wins:  " + str(results[2]) + "]                ", flush=True)
            print(args.engine1 + " vs " + args.engine2 + " " +  calculateEloDifference(results) + "                           ")
            print(str(round(((results[0]+results[1]+results[2])/int(args.games))*100, 1)) + "% complete                       ")

    print("")

    while (not q.empty()):
        r = q.get()
        results[0] += r[0]
        results[1] += r[1]
        results[2] += r[2]

    print("Final Score:  [" + args.engine1 + " wins:  " + str(results[0]) 
            + " | draws:  " + str(results[1]) + " | " + args.engine2 + " wins:  " + str(results[2]) + "]")
    print(args.engine1 + " vs " + args.engine2 + " " +  calculateEloDifference(results))


if (__name__ == "__main__"):
    main()
