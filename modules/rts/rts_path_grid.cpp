#include "rts_internal.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace rts {

namespace {

struct NodeCost {
	int index = 0;
	int estimate = 0;
	int cost = 0;
};

struct NodeCostGreater {
	bool operator()( const NodeCost &a, const NodeCost &b ) const {
		if ( a.estimate != b.estimate ) {
			return a.estimate > b.estimate;
		}
		return a.cost > b.cost;
	}
};

static int ManhattanDistance( int ax, int ay, int bx, int by ) {
	const int dx = ax > bx ? ax - bx : bx - ax;
	const int dy = ay > by ? ay - by : by - ay;
	return dx + dy;
}

static bool IsInBounds( int x, int y, int width, int height ) {
	return x >= 0 && y >= 0 && x < width && y < height;
}

static int GridIndex( int x, int y, int width ) {
	return y * width + x;
}

static bool IsBlocked( const unsigned char *blocked, int index ) {
	return blocked && blocked[index] != 0;
}

static int CopyPath( const std::vector<int> &path, int width, int *outX, int *outY, int maxOut ) {
	const int pathLength = static_cast<int>( path.size() );
	const int copyCount = std::min( pathLength, std::max( maxOut, 0 ) );

	for ( int i = 0; i < copyCount; ++i ) {
		const int index = path[static_cast<std::size_t>( i )];
		if ( outX ) {
			outX[i] = index % width;
		}
		if ( outY ) {
			outY[i] = index / width;
		}
	}

	return pathLength;
}

}  // namespace

int FindGridPath( int width, int height, const unsigned char *blocked, int startX, int startY, int goalX, int goalY, int *outX, int *outY, int maxOut ) {
	constexpr int kMaxCells = 1024 * 1024;
	const int kUnvisited = std::numeric_limits<int>::max();
	const int kDirections[4][2] = {
		{ 1, 0 },
		{ 0, 1 },
		{ -1, 0 },
		{ 0, -1 },
	};

	if ( width <= 0 || height <= 0 || width > kMaxCells / height ) {
		return 0;
	}
	const int cellCount = width * height;
	if ( cellCount > kMaxCells ) {
		return 0;
	}
	if ( !IsInBounds( startX, startY, width, height ) || !IsInBounds( goalX, goalY, width, height ) ) {
		return 0;
	}

	const int startIndex = GridIndex( startX, startY, width );
	const int goalIndex = GridIndex( goalX, goalY, width );
	if ( IsBlocked( blocked, startIndex ) || IsBlocked( blocked, goalIndex ) ) {
		return 0;
	}

	std::vector<int> cameFrom( static_cast<std::size_t>( cellCount ), -1 );
	std::vector<int> bestCost( static_cast<std::size_t>( cellCount ), kUnvisited );
	std::priority_queue<NodeCost, std::vector<NodeCost>, NodeCostGreater> frontier;

	bestCost[static_cast<std::size_t>( startIndex )] = 0;
	frontier.push( NodeCost{ startIndex, ManhattanDistance( startX, startY, goalX, goalY ), 0 } );

	while ( !frontier.empty() ) {
		const NodeCost current = frontier.top();
		frontier.pop();

		if ( current.cost != bestCost[static_cast<std::size_t>( current.index )] ) {
			continue;
		}
		if ( current.index == goalIndex ) {
			break;
		}

		const int currentX = current.index % width;
		const int currentY = current.index / width;
		for ( const auto &direction : kDirections ) {
			const int nextX = currentX + direction[0];
			const int nextY = currentY + direction[1];
			if ( !IsInBounds( nextX, nextY, width, height ) ) {
				continue;
			}

			const int nextIndex = GridIndex( nextX, nextY, width );
			if ( IsBlocked( blocked, nextIndex ) ) {
				continue;
			}

			const int nextCost = current.cost + 1;
			if ( nextCost >= bestCost[static_cast<std::size_t>( nextIndex )] ) {
				continue;
			}

			bestCost[static_cast<std::size_t>( nextIndex )] = nextCost;
			cameFrom[static_cast<std::size_t>( nextIndex )] = current.index;
			frontier.push( NodeCost{ nextIndex, nextCost + ManhattanDistance( nextX, nextY, goalX, goalY ), nextCost } );
		}
	}

	if ( bestCost[static_cast<std::size_t>( goalIndex )] == kUnvisited ) {
		return 0;
	}

	std::vector<int> path;
	for ( int index = goalIndex; index != -1; index = cameFrom[static_cast<std::size_t>( index )] ) {
		path.push_back( index );
		if ( index == startIndex ) {
			break;
		}
	}

	if ( path.empty() || path.back() != startIndex ) {
		return 0;
	}

	std::reverse( path.begin(), path.end() );
	return CopyPath( path, width, outX, outY, maxOut );
}

}  // namespace rts

extern "C" {

int RTS_FindGridPath( int width, int height, const unsigned char *blocked, int startX, int startY, int goalX, int goalY, int *outX, int *outY, int maxOut ) {
	return rts::FindGridPath( width, height, blocked, startX, startY, goalX, goalY, outX, outY, maxOut );
}

}
