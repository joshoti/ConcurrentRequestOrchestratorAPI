# Docker Deployment Guide

## 🐳 Architecture Overview

```
Frontend (React/Vue/etc)
    ↓
    WebSocket/HTTP
    ↓
Backend Docker Container (C server)
```

## 📋 Prerequisites

1. Docker and Docker Compose installed
2. DockerHub account
3. GitHub repository secrets configured

## 🔧 Setup GitHub Secrets

Go to your GitHub repository → Settings → Secrets and variables → Actions

Add these secrets:
- `DOCKERHUB_USERNAME` - Your DockerHub username
- `DOCKERHUB_TOKEN` - DockerHub access token (create at hub.docker.com/settings/security)

## 🚀 CI/CD Workflow

The GitHub Actions workflow automatically:
1. ✅ Runs all unit tests
2. ✅ Builds the project
3. ✅ Creates Docker image (only on push to main/develop)
4. ✅ Pushes to DockerHub with tags:
   - `latest` (main branch)
   - `develop` (develop branch)
   - `main-<commit-sha>` or `develop-<commit-sha>`

## 🏃 Running the Backend Locally

### Option 1: Using Docker Compose (Recommended)

```bash
# Set your DockerHub username
export DOCKERHUB_USERNAME=your-username

# Pull and run the backend
docker-compose up -d

# View logs
docker-compose logs -f request-orchestrator

# Stop
docker-compose down
```

### Option 2: Docker Run

```bash
# Pull the image
docker pull your-username/request-orchestrator:latest

# Run the container
docker run -d \
  --name request-orchestrator \
  -p 8000:8000 \
  your-username/request-orchestrator:latest

# View logs
docker logs -f request-orchestrator

# Stop
docker stop request-orchestrator
docker rm request-orchestrator
```

## 🌐 Frontend Integration

### Environment Variables (for your frontend)

```env
# .env.local or .env.production
VITE_BACKEND_WS_URL=ws://localhost:8000/ws/simulation
VITE_BACKEND_API_URL=http://localhost:8000/api
```

### Docker Compose with Frontend

Uncomment the frontend service in `docker-compose.yml` and customize:

```yaml
orchestrator-frontend:
  image: your-username/orchestrator-frontend:latest
  ports:
    - "3000:3000"
  environment:
    - BACKEND_URL=http://request-orchestrator:8000
  depends_on:
    request-orchestrator:
      condition: service_healthy
```

### Quick Start Script for Frontend Developers

Create `start-backend.sh` in your frontend repo:

```bash
#!/bin/bash
# Quick start script for frontend developers

DOCKERHUB_USERNAME="your-username"

echo "🐳 Starting backend container..."

# Pull latest image
docker pull ${DOCKERHUB_USERNAME}/request-orchestrator:latest

# Stop existing container if running
docker stop request-orchestrator 2>/dev/null || true
docker rm request-orchestrator 2>/dev/null || true

# Run new container
docker run -d \
  --name request-orchestrator \
  -p 8000:8000 \
  ${DOCKERHUB_USERNAME}/request-orchestrator:latest

# Wait for health check
echo "⏳ Waiting for backend to be healthy..."
sleep 3

# Check if running
if docker ps | grep -q request-orchestrator; then
  echo "✅ Backend is running!"
  echo "📡 WebSocket: ws://localhost:8000/ws/simulation"
  echo "🌐 HTTP API: http://localhost:8000/api/config"
  echo "🧪 Test page: http://localhost:8000/test_websocket.html"
else
  echo "❌ Backend failed to start. Check logs:"
  docker logs request-orchestrator
  exit 1
fi
```

## 🧪 Testing the Backend

```bash
# Test WebSocket connection
curl http://localhost:8000/api/config

# Or open in browser
open http://localhost:8000/test_websocket.html
```

## 📦 Building Custom Image Locally

```bash
# Build
docker build -t request-orchestrator:local .

# Run local build
docker run -d -p 8000:8000 request-orchestrator:local
```

## 🔍 Troubleshooting

**Container won't start:**
```bash
docker logs request-orchestrator
```

**Port already in use:**
```bash
# Use different port
docker run -d -p 8001:8000 your-username/request-orchestrator:latest
```

**Connection refused from frontend:**
- Check CORS is enabled (already configured in server.c)
- Ensure container is running: `docker ps`
- Check network: `docker network inspect bridge`

**Can't pull image:**
- Verify DockerHub username is correct
- Check image exists: https://hub.docker.com/r/your-username/request-orchestrator
- Ensure image is public or you're logged in: `docker login`

## 📊 Monitoring

```bash
# Container stats
docker stats request-orchestrator

# Health check
docker inspect request-orchestrator | grep -A 10 Health

# Restart if unhealthy
docker restart request-orchestrator
```

## 🔄 Updating

```bash
# Pull latest
docker-compose pull

# Restart with new image
docker-compose up -d

# Or with docker run
docker stop request-orchestrator
docker rm request-orchestrator
docker pull your-username/request-orchestrator:latest
docker run -d -p 8000:8000 --name request-orchestrator your-username/request-orchestrator:latest
```

## 🏗️ Development vs Production

**Development:**
- Use `develop` tag: `your-username/request-orchestrator:develop`
- Mount volumes for live config changes
- Enable debug mode

**Production:**
- Use `latest` tag or specific SHA
- Use orchestration (Kubernetes, Docker Swarm)
- Add reverse proxy (nginx, traefik)
- Enable TLS/SSL
- Set up monitoring and logging

## 📝 Architecture Benefits

- ✅ **Isolated environment** - No dependency conflicts
- ✅ **Version control** - Easy rollbacks with image tags
- ✅ **Portable** - Runs anywhere Docker runs
- ✅ **CI/CD ready** - Automated testing and deployment
- ✅ **Easy scaling** - Replicate containers easily
- ✅ **Team friendly** - Frontend devs can run backend instantly
