/**
   HobbyDrone Controller
   @author Kenta Suzuki
*/

#include <cnoid/SimpleController>
#include <cnoid/SharedJoystick>
#include <cnoid/EigenUtil>
#include <cnoid/RateGyroSensor>
#include <cnoid/Thruster>

using namespace std;
using namespace cnoid;

class HaiROVJoystickController : public SimpleController
{
public:
    SharedJoystickPtr joystick;
    int targetMode;
    BodyPtr ioBody;
    vector<Thruster*> thrusters;
    vector<Thruster*> pthrusters;
    RateGyroSensor* gyroSensor;
    double dt;

    Vector4 zref;
    Vector4 zprev;
    Vector4 dzref;
    Vector4 dzprev;

    Vector2 xyref;
    Vector2 xyprev;
    Vector2 dxyref;
    Vector2 dxyprev;

    bool on;
    bool manualMode;

    virtual bool initialize(SimpleControllerIO* io) override
    {
        ioBody = io->body();
        dt = io->timeStep();
        gyroSensor = ioBody->findDevice<RateGyroSensor>("GyroSensor");
        on = true;
        manualMode = false;

        for(auto opt : io->options()){
            if(opt == "manual"){
                manualMode = true;
            }
        }

        thrusters.clear();
        Thruster* thruster_rf = ioBody->findDevice<Thruster>("Thruster_RF");
        Thruster* thruster_lf = ioBody->findDevice<Thruster>("Thruster_LF");
        Thruster* thruster_lr = ioBody->findDevice<Thruster>("Thruster_LR");
        Thruster* thruster_rr = ioBody->findDevice<Thruster>("Thruster_RR");
        thrusters.push_back(thruster_rf);
        thrusters.push_back(thruster_lf);
        thrusters.push_back(thruster_lr);
        thrusters.push_back(thruster_rr);

        pthrusters.clear();
        Thruster* thruster_r = ioBody->findDevice<Thruster>("Thruster_R");
        Thruster* thruster_l = ioBody->findDevice<Thruster>("Thruster_L");
        pthrusters.push_back(thruster_r);
        pthrusters.push_back(thruster_l);

        io->enableInput(ioBody->rootLink(), LINK_POSITION);
        io->enableInput(gyroSensor);

        for(size_t i = 0; i < thrusters.size(); i++) {
            Thruster* thruster = thrusters[i];
            io->enableInput(thruster);
        }

        for(size_t i = 0; i < pthrusters.size(); ++i) {
            Thruster* thruster = pthrusters[i];
            io->enableInput(thruster);
        }

        zref = zprev = getZRPY();
        dzref = dzprev = Vector4::Zero();
        xyref = xyprev = getXY();
        dxyref = dxyprev = Vector2::Zero();

        joystick = io->getOrCreateSharedObject<SharedJoystick>("joystick");
        targetMode = joystick->addMode();

        io->os() << "Press the A button." << endl;

        return true;
    }

    virtual bool control() override
    {
        joystick->updateState(targetMode);

        static bool pprev = false;
        bool p = joystick->getButtonState(targetMode, Joystick::A_BUTTON);
        if(p && !pprev) {
            on = !on;
        }
        pprev = p;

        static const int mode1ID[] = {
            Joystick::R_STICK_V_AXIS,
            Joystick::L_STICK_H_AXIS,
            Joystick::L_STICK_V_AXIS,
            Joystick::R_STICK_H_AXIS
        };

        int axisID[4] = { 0 };
        for(int i = 0; i < 4; i++) {
            axisID[i] = mode1ID[i];
        }

        double ppos[2];
        for(int i=0; i < 2; ++i){
            ppos[i] = joystick->getPosition(targetMode,
                i==0 ? Joystick::L_STICK_H_AXIS : Joystick::L_STICK_V_AXIS);
            if(fabs(ppos[i]) < 0.2){
                ppos[i] = 0.0;
            }
        }

        if(on) {
            double k = 0.3;
            pthrusters[0]->force() = k * (-2.0 * ppos[1] - ppos[0]);
            pthrusters[1]->force() = k * (-2.0 * ppos[1] + ppos[0]);
            for(size_t i = 0; i < pthrusters.size(); ++i) {
                Thruster* thruster = pthrusters[i];
                thruster->notifyStateChange();
            }
        }

        static const double P[] = { 10.0, 1.0, 1.0, 0.01 };
        static const double D[] = { 5.0, 1.0, 1.0, 0.002 };
        static const double X[] = { -0.0001, 0.0, 0.0, 0.0 };

        static const double KP[] = { 1.0, 1.0 };
        static const double KD[] = { 1.0, 1.0 };
        static const double KX[] = { 0.0, 0.0 };

        static const double sign[4][4] = {
            { 1.0, -1.0, -1.0, -1.0 },
            { 1.0, 1.0, -1.0, 1.0 },
            { 1.0, 1.0, 1.0, -1.0 },
            { 1.0, -1.0, 1.0, 1.0 }
        };

        static const double dir[] = { -1.0, 1.0, -1.0, 1.0 };

        Vector4 f = Vector4::Zero();
        Vector4 z = getZRPY();
        Vector4 dz = (z - zprev) / dt;

        if(gyroSensor) {
            Vector3 w = ioBody->rootLink()->R() * gyroSensor->w();
            dz[3] = w[2];
        }

        Vector4 ddz = (dz - dzprev) / dt;

        Vector2 xy = getXY();
        Vector2 dxy = (xy - xyprev) / dt;
        Vector2 ddxy = (dxy - dxyprev) / dt;
        Vector2 dxy_local = Eigen::Rotation2Dd(-z[3]) * dxy;
        Vector2 ddxy_local = Eigen::Rotation2Dd(-z[3]) * ddxy;

        if(!on) {
            zref[0] = z[0];
            dzref[0] = dz[0];
        }

        for(int i = 0; i < 4; i++) {
            double pos = joystick->getPosition(targetMode, axisID[i]);
            if(fabs(pos) < 0.20) {
                pos = 0.0;
            }

            if(i == 3) {
                dzref[i] = X[i] * pos;
                f[i] = P[i] * (dzref[i] - dz[i]) + D[i] * (0.0 - ddz[i]);
            } else {
                if(i == 0) {
                    zref[i] += X[i] * pos;
                } else {
                    if(manualMode) {
                        zref[i] = X[i] * pos;
                    } else {
                        int j = i - 1;
                        dxyref[j] = KX[j] * pos;
                        zref[i] = KP[j] * (dxyref[j] - dxy_local[1 - j])
                                + KD[j] * (0.0 - ddxy_local[1 - j]);
                    }
                }
                if(i == 1) {
                    zref[i] *= -1.0;
                }
                f[i] = P[i] * (zref[i] - z[i]) + D[i] * (0.0 - dz[i]);
            }
        }

        zprev = z;
        dzprev = dz;
        xyprev = xy;
        dxyprev = dxy;

        for(size_t i = 0; i < thrusters.size(); i++) {
            Thruster* thruster = thrusters[i];
            double force = 0.0;
            if(on) {
                force += sign[i][0] * f[0];
                force += sign[i][1] * f[1];
                force += sign[i][2] * f[2];
                force += sign[i][3] * f[3];
            }
            thruster->force() = force;
            thruster->torque() = dir[i] * force;
            thruster->notifyStateChange();
        }

        return true;
    }

    Vector4 getZRPY()
    {
        auto T = ioBody->rootLink()->position();
        double z = T.translation().z();
        Vector3 rpy = rpyFromRot(T.rotation());
        return Vector4(z, rpy[0], rpy[1], rpy[2]);
    }
    Vector2 getXY()
    {
        auto p = ioBody->rootLink()->translation();
        return Vector2(p.x(), p.y());
    }
};

CNOID_IMPLEMENT_SIMPLE_CONTROLLER_FACTORY(HaiROVJoystickController)
