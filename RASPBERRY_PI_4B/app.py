# first install discord.py
# pip install discord.py

import discord
from discord.ext import commands
import RPi.GPIO as io

#Public Key -> '7dce84a70d3519d026a531bd1ce10473f8dc624723a15bb62e46d1d9217b2eec'

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
    
def get_temperature():
    return 0
    
bot.run("7dce84a70d3519d026a531bd1ce10473f8dc624723a15bb62e46d1d9217b2eec")