# first install discord.py
# pip install discord.py
import smbus
import discord
from discord.ext import commands
import RPi.GPIO as io

#Public Key -> '7dce84a70d3519d026a531bd1ce10473f8dc624723a15bb62e46d1d9217b2eec'

i2c_ch = 1
i2c_addr = 0x4d
reg_temp = 0x00
old_temp = 0
led = 3
io.setmode(io.BOARD)
io.setwarnings(False)
io.setup(led,io.OUT)
io.output(led,0)

bot = commands.Bot(command_prefix="/")

@bot.event
async def on_ready():
    print("bot is ready")
      
@bot.command()
async def led_on(ctx):
    io.output(led,1)
    await ctx.send("LED is On")
    
@bot.command()
async def led_off(ctx):
    io.output(led,0)
    await ctx.send("LED is Off")

@bot.command()
async def temperature(ctx):
    io.output(led,0)
    await ctx.send("Temperature is: ",get_temperature())

# Calculate the 2's complement of a number
def twos_comp(val, bits):
    if (val & (1 << (bits - 1))) != 0:
        val = val - (1 << bits)
    return val

def get_temperature():
    # Read temperature registers
    val = bus.read_i2c_block_data(i2c_addr, reg_temp, 2)
    # NOTE: val[0] = MSB byte 1, val [1] = LSB byte 2

    temp_c = (val[0] << 4) | (val[1] >> 4)

    # Convert to 2s complement (temperatures can be negative)
    temp_c = twos_comp(temp_c, 12)

    # Convert registers value to temperature (C)
    temp_c = temp_c * 0.0625

    return temp_c

    
bot.run("7dce84a70d3519d026a531bd1ce10473f8dc624723a15bb62e46d1d9217b2eec")

# Initialize I2C (SMBus)
bus = smbus.SMBus(i2c_ch)