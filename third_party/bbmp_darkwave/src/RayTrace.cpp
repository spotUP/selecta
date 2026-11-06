
#include "RayTrace.h"
#include "PI.h"
#include <math.h>

// speed of sound metres/second at 24 degrees celcius, 20% relative humidity 
#define SPEED_OF_SOUND 346.02f

#define DIRECTION_INC 0
#define DIRECTION_DEC 1
#define DIRECTION_STA 2

void RayTrace(double roomWidth, double roomHeight, double sourceX, double sourceY,
  int rayCount, double wallAbsorb, SRay **rays, int sampleRate, int samples)
{
  int   i, s;
  SRay *ray;
  double angle, angleInc, intensity, velocity;
  double xPos, yPos, xVelocity, yVelocity, xOverlap, yOverlap;
  int    xDirection, yDirection;
  bool   bouncing;

  // initialise rays
  
  *rays     = new SRay[rayCount];
  ray       = *rays;
  intensity = 1.0f / rayCount;
  angle     = 0.0f;
  angleInc  = (2.0 * PI) / rayCount;
  velocity  = SPEED_OF_SOUND / sampleRate;

  for(i = 0; i < rayCount; i++)
  {
    ray->intensity    = new double[samples];
    ray->xPos         = new double[samples];
    ray->yPos         = new double[samples];
    ray->intensity[0] = intensity;
    ray->xPos[0]      = sourceX;
    ray->yPos[0]      = sourceY;
    ray->xVelocity    = sin(angle) * velocity;
    ray->yVelocity    = cos(angle) * velocity;

    angle += angleInc;
    ray++;
  }

  // trace rays

  ray = *rays;
  for(i = 0; i < rayCount; i++)
  {
    intensity = ray->intensity[0];
    xPos      = ray->xPos[0];
    yPos      = ray->yPos[0];
    xVelocity = ray->xVelocity;
    yVelocity = ray->yVelocity;

    xDirection = xVelocity == 0.0f ? DIRECTION_STA : xVelocity < 0.0f ? DIRECTION_DEC : DIRECTION_INC;
    yDirection = yVelocity == 0.0f ? DIRECTION_STA : yVelocity < 0.0f ? DIRECTION_DEC : DIRECTION_INC;

    for(s = 1; s < samples; s++)
    {
      xPos += xVelocity;
      yPos += yVelocity;
      bouncing = true;

      while(bouncing)
      {
        xOverlap = xDirection == DIRECTION_INC ? xPos - roomWidth  : xDirection == DIRECTION_DEC ? -xPos : 0.0f;
        yOverlap = yDirection == DIRECTION_INC ? yPos - roomHeight : yDirection == DIRECTION_DEC ? -yPos : 0.0f;
    
        if(xOverlap >= yOverlap)
        {
          if(xOverlap > 0.0f)
          {
            if(xDirection == DIRECTION_INC)
            {
              xPos       = roomWidth - xOverlap;
              xDirection = DIRECTION_DEC;
            }
            else
            {
              xPos       = xOverlap;
              xDirection = DIRECTION_INC;
            }
            xVelocity  = -xVelocity;
            intensity  = intensity * wallAbsorb;
          }
          else
            bouncing = false;
        }
        else
        {
          if(yOverlap > 0.0f)
          {
            if(yDirection == DIRECTION_INC)
            {
              yPos       = roomHeight - yOverlap;
              yDirection = DIRECTION_DEC;
            }
            else
            {
              yPos       = yOverlap;
              yDirection = DIRECTION_INC;
            }
            yVelocity  = -yVelocity;
            intensity  = intensity * wallAbsorb;
          }
          else
            bouncing = false;
        }
      }
      ray->intensity[s] = intensity;
      ray->xPos[s]      = xPos;
      ray->yPos[s]      = yPos;
    }
    ray++;
  }
}

double ConvergeRays(int rayCount, SRay *rays, int sample, float x, float y, float r)
{
  int    i;
  SRay  *ray;
  double alpha, xd, yd, d2, r2, r2i;
 
  ray   = rays;
  alpha = 0.0f;
  r2    = r * r;
  r2i   = 1.0f / r2;

  for(i = 0; i < rayCount; i++)
  {
    xd = ray->xPos[sample] - x;
    yd = ray->yPos[sample] - y;
    d2 = (xd * xd) + (yd * yd);
    if(d2 <= r2)
      alpha += (r2 - d2) * r2i * ray->intensity[sample];
    ray++;
  }

  return alpha;
}