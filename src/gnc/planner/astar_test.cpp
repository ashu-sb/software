#include <common/grid_utils.hpp>
#include <common/timestamp.h>
#include <planning/motion_planner.hpp>
#include <slam/occupancy_grid.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

/*
* timing_info_t stores the timing information for the tests that are run.
*/
typedef std::map<std::string, std::vector<int64_t>> timing_info_t;
int gNumTestRepeats = 1;
timing_info_t gSuccess;     // Time to successfully find paths  HACK -- don't put global variables in your own code!
timing_info_t gFail;        // Time to find failures  HACK -- don't put global variables in your own code!


bool test_empty_grid(void);
bool test_filled_grid(void);
bool test_narrow_constriction_grid(void);
bool test_wide_constriction_grid(void);
bool test_convex_grid(void);
bool test_maze_grid(void);
bool test_saved_poses(const std::string& mapFile, const std::string& posesFile, const std::string& testName);

robot_path_t timed_find_path(const pose_xyt_t& start, 
                             const pose_xyt_t& end, 
                             const MotionPlanner& planner, 
                             const std::string& testName);

bool is_valid_path(const robot_path_t& path, double robotRadius, const OccupancyGrid& map);
bool is_safe_cell(int x, int y, double robotRadius, const OccupancyGrid& map);

std::ostream& operator<<(std::ostream& out, const pose_xyt_t& pose);

void print_timing_info(timing_info_t& info);


int main(int argc, char** argv)
{
    if(argc > 1)
    {
        gNumTestRepeats = std::max(std::atoi(argv[1]), 1);
        std::cout << "Running astar_test with " << gNumTestRepeats << " repeats for each planning problem.\n";
    }
    else
    {
        std::cout << "Running astar_test with " << gNumTestRepeats << " repeats for each planning problem.\nTo change this "
            << "number, specify the number of repeats on the command-line as:\n  ./astar_test <num repeats>  \nwhere "
            << "num_repeats is an integer > 0.\n";
    }
    
    typedef bool (*test_func) (void);
    std::vector<test_func> tests = { 
        test_empty_grid, //okay
        test_filled_grid, //okay
        test_narrow_constriction_grid,
        test_wide_constriction_grid,
        test_convex_grid,
        test_maze_grid
    };
    
    std::size_t numPassed = 0;
    for(auto& t : tests)
    {
        if(t())
        {
            ++numPassed;
        }
    }
    
    std::cout << "\nTiming information for successful planning attempts:\n";
    print_timing_info(gSuccess);
    
    std::cout << "\nTiming information for failed planning attempts:\n";
    print_timing_info(gFail);
    
    if(numPassed != tests.size())
    {
        std::cout << "\n\nINCOMPLETE: Passed " << numPassed << " of " << tests.size() 
        << " tests. Keep debugging and testing!\n";
    }
    else
    {
        std::cout << "\n\nCOMPLETE! All " << tests.size() << " were passed! Good job!\n";
    }
    
    return 0;
}


bool test_empty_grid(void)
{
    return test_saved_poses("../data/empty.map", "../data/empty_poses.txt", __FUNCTION__);
}


bool test_filled_grid(void)
{
    return test_saved_poses("../data/filled.map", "../data/filled_poses.txt", __FUNCTION__);
}


bool test_narrow_constriction_grid(void)
{
    return test_saved_poses("../data/narrow.map", "../data/narrow_poses.txt", __FUNCTION__);
}


bool test_wide_constriction_grid(void)
{
    return test_saved_poses("../data/wide.map", "../data/wide_poses.txt", __FUNCTION__);
}


bool test_convex_grid(void)
{
    return test_saved_poses("../data/convex.map", "../data/convex_poses.txt", __FUNCTION__);
}


bool test_maze_grid(void)
{
    return test_saved_poses("../data/maze.map", "../data/maze_poses.txt", __FUNCTION__);
}


bool test_saved_poses(const std::string& mapFile, const std::string& posesFile, const std::string& testName)
{
    std::cout << "\nSTARTING: " << testName << '\n';
    
    OccupancyGrid grid;
    grid.loadFromFile(mapFile);
    // printf("cells per meter in sest_saved_poses: %f", grid.cellsPerMeter());
    // return false;

    std::ifstream poseIn(posesFile);
    if(!poseIn.is_open())
    {
        std::cerr << "ERROR: No maze poses located in " << posesFile 
            << " Please run astar_test directly from the bin/ directory.\n";
        exit(-1);
    }
    
    int numGoals;
    poseIn >> numGoals;
    
    pose_xyt_t start;
    pose_xyt_t goal;
    start.theta = 0.0;
    goal.theta = 0.0;
    bool shouldExist;
    
    MotionPlannerParams plannerParams;
    plannerParams.robotRadius = 0.075;
    
    MotionPlanner planner(plannerParams);
    planner.setMap(grid);
    int numCorrect = 0;
    
    for(int n = 0; n < numGoals; ++n)
    {
        poseIn >> start.x >> start.y >> goal.x >> goal.y >> shouldExist;
        
        robot_path_t path = timed_find_path(start, goal, planner, testName);
        
        // See if the generated path was valid
        bool foundPath = path.path_length > 1;
        // The goal must be the same position as the end of the path if there was success
        if(!path.path.empty())
        {
            auto goalCell = global_position_to_grid_cell(Point<float>(goal.x, goal.y), grid);
            auto endCell = global_position_to_grid_cell(Point<float>(path.path.back().x, path.path.back().y), grid);
            foundPath &= goalCell == endCell;
        }
        
        if(foundPath)
        {
            if(shouldExist && is_valid_path(path, plannerParams.robotRadius, grid))
            {
                std::cout << "Correctly found path between start and goal: " << start << " -> " << goal << "\n";
                ++numCorrect;
            }
            else if(!shouldExist && is_valid_path(path, plannerParams.robotRadius, grid))
            {
                // DEBUG
                printf("Im here\n");
                printf("path length: %d\n", path.path_length);
                for(auto p : path.path)
                {
                    printf("pose, x:%f, y:%f\n", p.x, p.y);
                    auto cell = global_position_to_grid_cell(Point<float>(p.x, p.y), grid);
                    printf("distance: %f\n", planner.obstacleDistances().getDist(cell.x, cell.y));
                }

                std::cout << "Incorrectly found valid path between start and goal: " << start << " -> " << goal << "\n";
            }
            else if(shouldExist && !is_valid_path(path, plannerParams.robotRadius, grid))
            {
                // DEBUG
                for(auto p : path.path)
                {
                    printf("pose, x:%f, y:%f\n", p.x, p.y);
                    auto cell = global_position_to_grid_cell(Point<float>(p.x, p.y), grid);
                    printf("distance: %f\n", planner.obstacleDistances().getDist(cell.x, cell.y));
                }

                std::cout << "Incorrectly found unsafe path between start and goal: " << start << " -> " << goal 
                    << " Too close to obstacle!\n";
            }
            else
            {
                // DEBUG
                for(auto p : path.path)
                {
                    printf("pose, x:%f, y:%f\n", p.x, p.y);
                    auto cell = global_position_to_grid_cell(Point<float>(p.x, p.y), grid);
                    printf("distance: %f\n", planner.obstacleDistances().getDist(cell.x, cell.y));
                }
                std::cout << "Incorrectly found unsafe path between start and goal: " << start << " -> " << goal << "\n";
            }
        }
        else
        {
            if(shouldExist)
            {

                // DEBUG
                for(auto p : path.path)
                {
                    printf("pose, x:%f, y:%f\n", p.x, p.y);
                    auto cell = global_position_to_grid_cell(Point<float>(p.x, p.y), grid);
                    printf("distance: %f\n", planner.obstacleDistances().getDist(cell.x, cell.y));
                }

                std::cout << "Incorrectly found no path between start and goal: " << start << " -> " << goal << "\n";
            }
            else
            {
                std::cout << "Correctly found no path between start and goal: " << start << " -> " << goal << "\n";
                ++numCorrect;
            }
        }
    }
    
    if(numCorrect == numGoals)
    {
        std::cout << "PASSED! " << testName << '\n';
    }
    else
    {
        std::cout << "FAILED! " << testName << '\n';
    }
    
    return numCorrect == numGoals;
}


robot_path_t timed_find_path(const pose_xyt_t& start, 
                             const pose_xyt_t& end, 
                             const MotionPlanner& planner, 
                             const std::string& testName)
{
    // Perform each search many times to get better timing information
    robot_path_t path;
    for(int n = 0; n < gNumTestRepeats; ++n)
    {
        int64_t startTime = utime_now();
        path = planner.planPath(start, end);
        int64_t endTime = utime_now();
        
        if(path.path_length > 1)
        {
            gSuccess[testName].push_back(endTime - startTime);
        }
        else
        {
            gFail[testName].push_back(endTime - startTime);
        }
    }
    
    return path;
}


bool is_valid_path(const robot_path_t& path, double robotRadius, const OccupancyGrid& map)
{
    // If there's only a single entry, then it isn't a valid path
    if(path.path_length < 2)
    {
        return false;
    }
    
    // Look at each position in the path, along with any intermediate points between the positions to make sure they are
    // far enough from walls in the occupancy grid to be safe
    for(auto p : path.path)
    {
        auto cell = global_position_to_grid_cell(Point<float>(p.x, p.y), map);
        if(!is_safe_cell(cell.x, cell.y, robotRadius, map))
        {
            return false;
        }
    }
    
    return true;
}


bool is_safe_cell(int x, int y, double robotRadius, const OccupancyGrid& map)
{
    // Search a circular region around (x, y). If any of the cells within the robot radius are occupied, then the
    // cell isn't safe.
    const int kSafeCellRadius = std::lrint(std::ceil(robotRadius * map.cellsPerMeter()));
    
    for(int dy = -kSafeCellRadius; dy <= kSafeCellRadius; ++dy)
    {
        for(int dx = -kSafeCellRadius; dx <= kSafeCellRadius; ++dx)
        {
            // Ignore the corners of the square region, where outside the radius of the robot
            if(std::sqrt(dx*dx + dy*dy) * map.metersPerCell() > robotRadius)
            {
                continue;
            }
            
            // If the odds at the cells are greater than 0, then there's a collision, so the cell isn't safe
            if(map.logOdds(x + dx, y + dy) > 0)
            {
                // DEBUG
                printf("IM NOT SAFE GRID, x: %d, y: %d\n", x, y);
                auto global = grid_position_to_global_position(Point<int>(x, y), map);
                printf("IM NOT SAFE, x: %f, y: %f\n", global.x, global.y);
                auto hit = grid_position_to_global_position(Point<int>(x+dx, y+dy), map);
                printf("HIT POINT GRID: x: %d, y: %d\n", x+dx, y+dy);
                printf("HIT POINT: x: %f, y: %f\n", hit.x, hit.y);
                printf("ROBOT RADIUS: %f\n", robotRadius);
                return false;
            }
        }
    }
    
    // The area around the cell is free of obstacles, so all is well
    return true;
}


std::ostream& operator<<(std::ostream& out, const pose_xyt_t& pose)
{
    out << '(' << pose.x << ',' << pose.y << ',' << pose.theta << ')';
    return out;
}


void print_timing_info(timing_info_t& info)
{
    using namespace boost::accumulators;
    typedef accumulator_set<double, stats<tag::mean, tag::variance, tag::median, tag::max, tag::min>> TimingAcc;
    
    for(auto& times : info)
    {
        assert(!times.second.empty());
        
        TimingAcc acc;
        std::for_each(times.second.begin(), times.second.end(), std::ref(acc));
        
        std::cout << times.first << " :: (us)\n"
            << "\tMin :    " << extract::min(acc) << '\n'
            << "\tMean:    " << mean(acc) << '\n'
            << "\tMax:     " << extract::max(acc) << '\n'
            << "\tMedian:  " << median(acc) << '\n'
            << "\tStd dev: " << std::sqrt(variance(acc)) << '\n'; 
    }
}
