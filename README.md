# Puppy Pal

A virtual pet game for the M5StickC Plus2 featuring an adorable animated dog. Care for your puppy by feeding, playing, and keeping it happy and healthy!

## Features

- **Animated Dog Character**: Cute dog face with expressive eyes, mouth, and floppy ears
- **Pet Care System**: Manage hunger, happiness, energy, and health
- **Interactive Gameplay**: Feed, play, and put your pet to sleep
- **Animated States**: Different expressions for happy, hungry, sleeping, sick, and playing
- **Persistent Storage**: Pet stats are saved automatically and restored on startup
- **Power Management**: Auto-sleep feature to conserve battery with periodic health checks
- **Shake Detection**: Shaking the device can wake your pet or make it sick
- **Sound Effects**: Audio feedback for various actions and states
- **Battery Monitoring**: Real-time battery percentage display
- **Stats Screen**: View detailed pet information and overall health status

## Hardware Requirements

- M5StickC Plus2 device
- USB-C cable for programming and charging
- Arduino IDE with M5Stack board package installed

## Controls

### Short Press Button A (Front/M5 Button)
- Open menu when pet is idle
- Select menu item
- Quick feed when held for 1+ second

### Short Press Button B (Side Button)
- Navigate menu items (up/down)
- Wake pet from sleep
- Put pet to sleep when held for 1.5+ seconds

### Menu Navigation
- Use Button B to cycle through menu options
- Press Button A to select an option
- Menu automatically closes after 5 seconds of inactivity

### Reset Pet
- Hold both Button A and Button B simultaneously during power-on to reset pet to default state

## Pet Stats

Your pet has four main statistics that you need to maintain:

### Hunger (0-100)
- Indicates how full your pet's stomach is
- Decreases over time
- Feed your pet when it gets low
- Value of 100 means completely full

### Happiness (0-100)
- Reflects your pet's emotional state
- Decreases when hungry, tired, or neglected
- Increases when playing or being fed
- Higher happiness means a healthier pet

### Energy (0-100)
- Shows how rested your pet is
- Decreases throughout the day and during play
- Restores while sleeping
- Low energy prevents playing

### Health (0-100)
- Overall physical condition of your pet
- Decreases when other stats are critically low
- Increases when all stats are maintained well
- Critical health needs immediate attention

### Age
- Tracks how long you've cared for your pet
- Measured in minutes and hours
- Persists between sessions

## Game Mechanics

### Stat Decay
- Stats decrease gradually over time
- Hunger, happiness, and energy decay every 10 seconds when awake
- Health decays only when other stats are critically low (<20)
- Health recovers when all stats are well maintained (>60 happiness, >60 hunger, >40 energy)

### Feeding
- Opens from menu or quick-feed with long press A
- Increases hunger by 25 points
- Slightly increases happiness by 5 points
- Cannot feed if hunger is already at 100%
- Watch the eating animation as your pet enjoys its meal

### Playing
- Opens from menu selection
- Increases happiness by 20 points
- Decreases energy by 15 points
- Slightly decreases hunger by 5 points
- Cannot play if energy is below 20%
- Watch your pet bounce and look around happily

### Sleeping
- Put to sleep by long-pressing Button B
- Screen dims to conserve power
- Energy restores at 5 points every 10 seconds
- Wake by pressing Button B or shaking the device
- Good for restoring energy quickly

### Sickness
- Pet gets sick if you shake the device while it's awake
- Causes pet to vomit with animation
- Decreases health by 15 points
- Decreases happiness by 20 points
- Pet recovers automatically when health >60, happiness >50, and energy >40
- Avoid shaking to keep your pet healthy

## Power Management

### Auto-Sleep
- Device enters deep sleep after 60 seconds of inactivity in idle state
- Does not auto-sleep during active states (eating, playing, etc.)
- Saves battery significantly

### Deep Sleep Mode
- Display turns off completely
- Wakes every 15 minutes to check if pet needs attention
- If any stat is below 25%, plays alert sound and shows which stat is low
- Press any button to care for your pet and stop alerts
- If no response after 30 seconds, goes back to sleep

### Wake Sources
- Button A press
- Timer (every 15 minutes for health check)
- Device powers on from deep sleep when woken

### Battery Saving Tips
- Pet auto-sleeps when you're not interacting
- Deep sleep preserves battery for extended periods
- Check battery level in stats screen
- Charge via USB-C cable when battery is low

## Installation

1. Install the Arduino IDE from https://www.arduino.cc/en/software
2. Add M5Stack board support:
   - Open Arduino IDE
   - Go to File -> Preferences
   - Add `https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json` to "Additional Boards Manager URLs"
3. Install M5Stack board package:
   - Go to Tools -> Board -> Boards Manager
   - Search for "M5Stack"
   - Install "M5Stack Board Manager"
4. Install required libraries:
   - Go to Sketch -> Include Library -> Manage Libraries
   - Install "M5StickCPlus2"
   - Install "Preferences"
5. Select your board:
   - Tools -> Board -> M5Stack -> M5StickC Plus2
6. Connect M5StickC Plus2 via USB-C
7. Select correct port in Tools -> Port
8. Open main.ino in Arduino IDE
9. Click Upload button to flash firmware

## Reset Instructions

To reset your pet to default state (clears all saved progress):

1. Power off the M5StickC Plus2
2. Hold both Button A and Button B simultaneously
3. Power on the device while keeping both buttons pressed
4. Release buttons when "RESETTING PET..." appears on screen
5. Wait for "Done! New pet!" message
6. Your pet will start with default stats (80 hunger, 80 happiness, 100 energy, 100 health)

## Pet Care Tips

### Daily Maintenance
- Feed your pet when hunger drops below 50%
- Play with your pet to keep happiness high
- Let your pet sleep when energy is low
- Check stats screen regularly to monitor overall health

### Keeping Your Pet Healthy
- Maintain all stats above 60% for optimal health
- Don't let any stat drop below 25% to prevent health loss
- Feed before playing to avoid hunger issues
- Balance playtime with rest periods

### Avoiding Sickness
- Don't shake the device unnecessarily
- Keep your pet well-fed and happy
- Address low stats promptly before they affect health

### Battery Life
- Let the device auto-sleep when not in use
- Check battery level periodically in stats screen
- Charge when battery percentage drops below 20%
- The pet will alert you if it needs care during sleep

### Progress Tips
- Pet age accumulates over time, showing your care history
- Stats are saved automatically every 30 seconds
- Time-based stat decay is applied when waking from deep sleep
- A well-cared-for pet can live indefinitely!

## Troubleshooting

### Pet stats not saving
- Ensure the device has sufficient battery
- Check that Preferences library is properly installed
- Try resetting the pet and starting fresh

### Device won't wake from sleep
- Press Button A firmly
- If unresponsive, connect USB-C cable to charge
- Fully power off and restart if needed

### Screen flickering
- This is normal during state transitions
- Should resolve quickly (within 1 second)
- If persistent, check battery level

### Sound not playing
- Check that speaker is enabled in setup
- Ensure device volume is not muted
- Restart device if sound issues persist

## License

This project is open source and available for personal use and modification.

## Author

Created for the M5StickC Plus2 platform as a fun and engaging virtual pet experience.