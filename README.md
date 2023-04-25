# A multi robot communication service base on Zenoh

## **Requirements**

 * Python 3.9 minimum
 * A [zenoh router](http://zenoh.io/docs/getting-started/quick-test/)
 * The [zenoh/DDS bridge](https://github.com/eclipse-zenoh/zenoh-plugin-dds#trying-it-out)
 * [zenoh-python](https://github.com/eclipse-zenoh/zenoh-python): install it with `pip install eclipse-zenoh`.
 * [pycdr2](https://pypi.org/project/pycdr2/): install it with `pip install pycdr2`.


-----
## **Test**

 1. Launch robot launcher on turtlebo3:
      
      ros2 launch turtlebot3_bringup robot.launch.py
      
 2. Start the zenoh router on V2x module:
      
      zenohd
      
 3. Start the zenoh/DDS bridge on turtlebot3:
      
      zenoh-bridge-dds -e tcp/$(V2x module IP):7447
     
 4. Start multi robot status manager on V2x module:
      
      python3 multi_robot_manager.py -e tcp/$(V2x module IP):7447
     
 5. Recieve robot status at your own PC #Note: make sure your PC is on the same Network region with V2x module
     
      python3 zenoh_test.py -e tcp/$(V2x module IP):7447






