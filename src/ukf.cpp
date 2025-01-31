#include "ukf.h"
#include "Eigen/Dense"

using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 3.0; //play with this

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.6; //play with this
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */
  time_us_ = 0.0;
  isInitialized_ = false;
  n_aug_ = 7;
  n_x_ = 5;  
  lambda_ = 3 - n_aug_;

  Xsig_pred_ = MatrixXd(n_x_, 2*n_aug_+1);
  weights_ = VectorXd(2*n_aug_+1);
  
  HLidar_ = MatrixXd(2,5);
  HLidar_ << 1, 0, 0, 0, 0,
              0, 1, 0, 0, 0;

  RLidar_ = MatrixXd(2,2);
  RLidar_ << std_laspx_*std_laspx_, 0,
                                  0, std_laspy_*std_laspy_;

  RRadar_ = MatrixXd(3,3);
  RRadar_ << std_radr_*std_radr_, 0, 0,
                                  0, std_radphi_*std_radphi_, 0,
                                  0, 0, std_radrd_*std_radrd_;
}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */
  if (isInitialized_ == false) {
    std::cout << "Starting the UKF" << std::endl;

    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      std::cout << "INIT RADAR: " << std::endl;
      double rho = meas_package.raw_measurements_(0);
      double phi = meas_package.raw_measurements_(1);
      double rhod = meas_package.raw_measurements_(2);
      x_ << rho * cos(phi),
            rho * sin(phi),
            0.0, 0.0, 0.0;
      P_ = MatrixXd::Identity(5,5);
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      std::cout << "INIT LASER: " << std::endl;
      x_ << meas_package.raw_measurements_(0),
            meas_package.raw_measurements_(1),
            0.0, 0.0, 0.0;
      P_ = MatrixXd::Identity(5,5);
      P_(0,0) = std_laspx_*std_laspx_;
      P_(1,1) = std_laspy_*std_laspy_;
    }

    VectorXd weights = VectorXd(2*n_aug_ + 1);
    weights(0) = lambda_ / (lambda_ + n_aug_);
    for (int i=1; i < weights_.size(); i++) {
        weights(i) = 0.5/(n_aug_+lambda_);
    }
    weights_ = weights;

    time_us_ = meas_package.timestamp_;

    isInitialized_ = true;
    return;
  }
  
  double delta_t = (double) ((meas_package.timestamp_ - time_us_) * 1e-6);
  time_us_ = meas_package.timestamp_;
  Prediction(delta_t);

  if (use_radar_ && meas_package.sensor_type_ == MeasurementPackage::RADAR) {
    UpdateRadar(meas_package);
  }
  else if (use_laser_ && meas_package.sensor_type_ == MeasurementPackage::LASER) {
    UpdateLidar(meas_package);
  }
}

void UKF::Prediction(double delta_t) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */

  VectorXd x_aug = VectorXd(n_aug_);
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  x_aug.head(n_x_) = x_;
  x_aug(5) = x_aug(6) = 0.0;
  P_aug.fill(0.0);
  P_aug.topLeftCorner(n_x_, n_x_) = P_;
  P_aug(5,5) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;
  MatrixXd A = P_aug.llt().matrixL();
  Xsig_aug.col(0) = x_aug;
  for (int i = 0; i < n_aug_; ++i) {
    Xsig_aug.col(i+1)     = x_aug + sqrt(lambda_+n_aug_) * A.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * A.col(i);
  }

  for (int j=0; j < Xsig_aug.cols(); j++) {
      double yaw = Xsig_aug(3,j);
      double yawd = Xsig_aug(4,j);
      double nu_a = Xsig_aug(5,j);
      double nu_yawdd = Xsig_aug(6,j);
      double p_x = Xsig_aug(0,j);
      double p_y = Xsig_aug(1,j);
      double v = Xsig_aug(2,j);

			double px_p, py_p;

			if (fabs(yawd) > 0.001) {
					px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
					py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
			} else {
					px_p = p_x + v*delta_t*cos(yaw);
					py_p = p_y + v*delta_t*sin(yaw);
			}

			double v_p = v;
			double yaw_p = yaw + yawd*delta_t;
			double yawd_p = yawd;

			px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
			py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
			v_p = v_p + nu_a*delta_t;

			yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
			yawd_p = yawd_p + nu_yawdd*delta_t;

			Xsig_pred_(0,j) = px_p;
			Xsig_pred_(1,j) = py_p;
			Xsig_pred_(2,j) = v_p;
			Xsig_pred_(3,j) = yaw_p;
			Xsig_pred_(4,j) = yawd_p;
  }

  VectorXd x_pred = VectorXd(n_x_);
  MatrixXd P_pred = MatrixXd(n_x_, n_x_);
  x_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      x_pred = x_pred + weights_(i) * Xsig_pred_.col(i);
  }
  P_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      VectorXd center = Xsig_pred_.col(i) - x_;
			while (center(3)> M_PI) center(3)-=2.*M_PI;
			while (center(3)<-M_PI) center(3)+=2.*M_PI;
      P_pred += weights_(i) * center * center.transpose();
  }

  x_ = x_pred;
  P_ = P_pred;
}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */

  VectorXd z = VectorXd(2);
  z << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1];
  VectorXd z_pred = HLidar_ * x_;
  VectorXd y = z - z_pred;

  MatrixXd S = HLidar_ * P_ * HLidar_.transpose() + RLidar_;
  MatrixXd K = P_ * HLidar_.transpose() * S.inverse();

  x_ = x_ + K * y;
  x_(3) = NormalizePI(x_(3));

  P_ = (MatrixXd::Identity(n_x_, n_x_) - K * HLidar_) * P_;
}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */

  VectorXd z = VectorXd(3);
  z << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1], meas_package.raw_measurements_[2];
    
  MatrixXd Zsig = MatrixXd(3, 2*n_aug_ + 1);
  VectorXd z_pred = VectorXd(3);
  MatrixXd S = MatrixXd(3, 3);

	for(int i=0; i < 2*n_aug_+1; i++) {
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                       // r
    Zsig(1,i) = atan2(p_y,p_x);                                // phi
    Zsig(2,i) = (p_x*v1 + p_y*v2) / sqrt(p_x*p_x + p_y*p_y);   // r_dot
	}

  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; ++i) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    z_diff(1) = NormalizePI(z_diff(1));

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }
  S = S + RRadar_;
  
  MatrixXd Tc = MatrixXd(n_x_, 3);
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    z_diff(1) = NormalizePI(z_diff(1));
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    x_diff(3) = NormalizePI(x_diff(3));

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  MatrixXd K = Tc * S.inverse();
  VectorXd z_diff = z - z_pred;
  z_diff(1) = NormalizePI(z_diff(1));

  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();
}

double UKF::NormalizePI(double angle) {
  while (angle> M_PI)
    angle-=2.*M_PI;
  while (angle<-M_PI)
    angle+=2.*M_PI;
  return angle;
}