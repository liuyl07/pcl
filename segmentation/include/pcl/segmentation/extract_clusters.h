/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 *
 */

#pragma once

#include <pcl/pcl_base.h>

#include <pcl/common/utils.h>
#include <pcl/search/pcl_search.h>

namespace pcl
{

namespace detail {
template <typename PointT, typename Function>
constexpr static bool is_functor_for_additional_filter_criteria_v =
    pcl::is_invocable_r_v<bool,
                          Function,
                          const pcl::remove_cvref_t<pcl::PointCloud<PointT>>&,
                          pcl::index_t,
                          const pcl::remove_cvref_t<Indices>&,
                          pcl::index_t>;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Decompose a region of space into clusters based on the Euclidean distance between points
  * \param it cloud iterator for iterating over the points
  * \param cloud the point cloud message
  * \param additional_filter_criteria a functor specifying a criterion that must be satisfied by all point added to the cluster
  * \param tree the spatial locator (e.g., kd-tree) used for nearest neighbors searching
  * \note the tree has to be created as a spatial locator on \a cloud and \a indices
  * \param tolerance the spatial cluster tolerance as a measure in L2 Euclidean space
  * \param clusters the resultant clusters containing point indices (as a vector of PointIndices)
  * \param min_pts_per_cluster minimum number of points that a cluster may contain (default: 1)
  * \param max_pts_per_cluster maximum number of points that a cluster may contain (default: max int)
  * \warning The cloud/indices passed here must be the same ones used to build the tree.
  *  For performance reasons PCL on warns if the sizes are not equal and no checks are performed to verify if the content is the same
  * \ingroup segmentation
  */
  template <typename PointT, typename FunctorT> void
   extractEuclideanClusters(
      ConstCloudIterator<PointT> &it, const PointCloud<PointT> &cloud, FunctorT additional_filter_criteria,
      const typename search::Search<PointT>::Ptr &tree, float tolerance, std::vector<PointIndices> &clusters,
      unsigned int min_pts_per_cluster = 1, unsigned int max_pts_per_cluster = std::numeric_limits<int>::max())
  {
    static_assert(detail::is_functor_for_additional_filter_criteria_v<PointT, FunctorT>,
                  "Functor signature must be similar to `bool(const PointCloud<PointT>&, "
                  "index_t, const Indices&, index_t)`");

    if (tree->getInputCloud ()->points.size () != cloud.points.size ())
    {
      PCL_ERROR ("[pcl::extractEuclideanClusters] Tree built with a different point cloud size (%lu) than the input cloud (%lu)!\n", tree->getInputCloud ()->points.size (), cloud.points.size ());
      return;
    }

    // Check if the tree is sorted -- if it is we don't need to check the first element
    index_t nn_start_idx = tree->getSortedResults () ? 1 : 0;
    // Create a bool vector of processed point indices, and initialize it to false
    std::vector<bool> processed (cloud.points.size (), false);

    for (; it.isValid(); ++it) {
      if (processed[it.getCurrentIndex()])
        continue;

      clusters.emplace_back();
      auto& seed_queue = clusters.back();
      seed_queue.indices.push_back (it.getCurrentIndex());

      processed[it.getCurrentIndex()] = true;

      for (index_t sq_idx = 0; sq_idx < static_cast<index_t> (seed_queue.indices.size()); ++sq_idx)
      {
        Indices nn_indices;
        std::vector<float> nn_distances;

        // Search for sq_idx
        if (!tree->radiusSearch (seed_queue.indices[sq_idx], tolerance, nn_indices, nn_distances))
          continue;

        for (index_t j = nn_start_idx; j < nn_indices.size (); ++j) // can't assume sorted (default isn't!)
        {
          if (processed[nn_indices[j]]) // Has this point been processed before ?
            continue;

          if (additional_filter_criteria(cloud, it.getCurrentIndex(), nn_indices, j)) {
            seed_queue.indices.push_back(nn_indices[j]);
            processed[nn_indices[j]] = true;
          }
        }
      }

      // If this queue is satisfactory, add to the clusters
      if (seed_queue.indices.size () >= min_pts_per_cluster && seed_queue.indices.size () <= max_pts_per_cluster)
        seed_queue.header = cloud.header;
      else
        clusters.pop_back();
    }
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Decompose a region of space into clusters based on the Euclidean distance between points
    * \param cloud the point cloud message
    * \param additional_filter_criteria a functor specifying a criterion that must be satisfied by all point added to the cluster
    * \param tree the spatial locator (e.g., kd-tree) used for nearest neighbors searching
    * \note the tree has to be created as a spatial locator on \a cloud and \a indices
    * \param tolerance the spatial cluster tolerance as a measure in L2 Euclidean space
    * \param clusters the resultant clusters containing point indices (as a vector of PointIndices)
    * \param min_pts_per_cluster minimum number of points that a cluster may contain (default: 1)
    * \param max_pts_per_cluster maximum number of points that a cluster may contain (default: max int)
    * \warning The cloud/indices passed here must be the same ones used to build the tree.
    *  For performance reasons PCL on warns if the sizes are not equal and no checks are performed to verify if the content is the same
    * \ingroup segmentation
    */
  template <typename PointT, typename FunctorT> void
  extractEuclideanClusters (
      const PointCloud<PointT> &cloud,
      FunctorT additional_filter_criteria, const typename search::Search<PointT>::Ptr &tree,
      float tolerance, std::vector<PointIndices> &clusters,
      unsigned int min_pts_per_cluster = 1, unsigned int max_pts_per_cluster = std::numeric_limits<int>::max())
  {
    auto it = ConstCloudIterator<PointT>(cloud);
    extractEuclideanClusters(it, cloud, additional_filter_criteria, tree, tolerance, clusters, min_pts_per_cluster, max_pts_per_cluster);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Decompose a region of space into clusters based on the Euclidean distance between points
    * \param cloud the point cloud message
    * \param indices a list of point indices to use from \a cloud
    * \param additional_filter_criteria a functor specifying a criterion that must be satisfied by all point added to the cluster
    * \param tree the spatial locator (e.g., kd-tree) used for nearest neighbors searching
    * \note the tree has to be created as a spatial locator on \a cloud and \a indices
    * \param tolerance the spatial cluster tolerance as a measure in L2 Euclidean space
    * \param clusters the resultant clusters containing point indices (as a vector of PointIndices)
    * \param min_pts_per_cluster minimum number of points that a cluster may contain (default: 1)
    * \param max_pts_per_cluster maximum number of points that a cluster may contain (default: max int)
    * \warning The cloud/indices passed here must be the same ones used to build the tree.
    *  For performance reasons PCL on warns if the sizes are not equal and no checks are performed to verify if the content is the same
    * \ingroup segmentation
    */
  template <typename PointT, typename FunctorT> void
  extractEuclideanClusters (
      const PointCloud<PointT> &cloud, const Indices &indices,
      FunctorT additional_filter_criteria, const typename search::Search<PointT>::Ptr &tree,
      float tolerance, std::vector<PointIndices> &clusters,
      unsigned int min_pts_per_cluster = 1, unsigned int max_pts_per_cluster = std::numeric_limits<int>::max())
  {
    // \note If the tree was created over <cloud, indices>, we guarantee a 1-1 mapping between what the tree returns
    //and indices[i]
    if (tree->getIndices ()->size () != indices.size ())
    {
      PCL_ERROR ("[pcl::extractEuclideanClusters] Tree built with a different size of indices (%lu) than the input set (%lu)!\n", tree->getIndices ()->size (), indices.size ());
      return;
    }

    auto it = ConstCloudIterator<PointT>(cloud, indices);
    extractEuclideanClusters(it, cloud, additional_filter_criteria, tree, tolerance, clusters, min_pts_per_cluster, max_pts_per_cluster);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Decompose a region of space into clusters based on the Euclidean distance between points
    * \param cloud the point cloud message
    * \param tree the spatial locator (e.g., kd-tree) used for nearest neighbors searching
    * \note the tree has to be created as a spatial locator on \a cloud
    * \param tolerance the spatial cluster tolerance as a measure in L2 Euclidean space
    * \param clusters the resultant clusters containing point indices (as a vector of PointIndices)
    * \param min_pts_per_cluster minimum number of points that a cluster may contain (default: 1)
    * \param max_pts_per_cluster maximum number of points that a cluster may contain (default: max int)
    * \ingroup segmentation
    */
  template <typename PointT> void
  extractEuclideanClusters (
      const PointCloud<PointT> &cloud, const typename search::Search<PointT>::Ptr &tree,
      float tolerance, std::vector<PointIndices> &clusters,
      unsigned int min_pts_per_cluster = 1, unsigned int max_pts_per_cluster = std::numeric_limits<int>::max ());

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Decompose a region of space into clusters based on the Euclidean distance between points
    * \param cloud the point cloud message
    * \param indices a list of point indices to use from \a cloud
    * \param tree the spatial locator (e.g., kd-tree) used for nearest neighbors searching
    * \note the tree has to be created as a spatial locator on \a cloud and \a indices
    * \param tolerance the spatial cluster tolerance as a measure in L2 Euclidean space
    * \param clusters the resultant clusters containing point indices (as a vector of PointIndices)
    * \param min_pts_per_cluster minimum number of points that a cluster may contain (default: 1)
    * \param max_pts_per_cluster maximum number of points that a cluster may contain (default: max int)
    * \ingroup segmentation
    */
  template <typename PointT> void 
  extractEuclideanClusters (
      const PointCloud<PointT> &cloud, const Indices &indices,
      const typename search::Search<PointT>::Ptr &tree, float tolerance, std::vector<PointIndices> &clusters,
      unsigned int min_pts_per_cluster = 1, unsigned int max_pts_per_cluster = std::numeric_limits<int>::max());

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Decompose a region of space into clusters based on the euclidean distance between points, and the normal
    * angular deviation
    * \param cloud the point cloud message
    * \param normals the point cloud message containing normal information
    * \param tree the spatial locator (e.g., kd-tree) used for nearest neighbors searching
    * \note the tree has to be created as a spatial locator on \a cloud
    * \param tolerance the spatial cluster tolerance as a measure in the L2 Euclidean space
    * \param clusters the resultant clusters containing point indices (as a vector of PointIndices)
    * \param max_angle the maximum allowed difference between normals in radians for cluster/region growing
    * \param min_pts_per_cluster minimum number of points that a cluster may contain (default: 1)
    * \param max_pts_per_cluster maximum number of points that a cluster may contain (default: max int)
    * \ingroup segmentation
    */
  template <typename PointT, typename Normal> void 
  extractEuclideanClusters (
      const PointCloud<PointT> &cloud, const PointCloud<Normal> &normals,
      float tolerance, const typename search::Search<PointT>::Ptr &tree,
      std::vector<PointIndices> &clusters, double max_angle,
      unsigned int min_pts_per_cluster = 1,
      unsigned int max_pts_per_cluster = (std::numeric_limits<int>::max) ())
  {
    if (cloud.points.size () != normals.points.size ())
    {
      PCL_ERROR ("[pcl::extractEuclideanClusters] Number of points in the input point cloud (%lu) different than normals (%lu)!\n", cloud.points.size (), normals.points.size ());
      return;
    }

    max_angle = std::min(std::abs(max_angle), M_PI);
    auto cos_max_angle = std::cos(max_angle);
    auto normal_deviation_filter = [&](const PointCloud<PointT> &cloud, index_t i, const Indices& nn_indices, index_t j) -> bool {
      double dot_p = normals[i].getNormalVector3fMap().dot(normals[nn_indices[j]].getNormalVector3fMap());
      return std::abs(dot_p) < cos_max_angle;
    };
    pcl::extractEuclideanClusters(cloud, normal_deviation_filter, tree, tolerance, clusters, min_pts_per_cluster, max_pts_per_cluster);
  }


  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Decompose a region of space into clusters based on the euclidean distance between points, and the normal
    * angular deviation
    * \param cloud the point cloud message
    * \param normals the point cloud message containing normal information
    * \param indices a list of point indices to use from \a cloud
    * \param tree the spatial locator (e.g., kd-tree) used for nearest neighbors searching
    * \note the tree has to be created as a spatial locator on \a cloud
    * \param tolerance the spatial cluster tolerance as a measure in the L2 Euclidean space
    * \param clusters the resultant clusters containing point indices (as PointIndices)
    * \param max_angle the maximum allowed difference between normals in degrees for cluster/region growing
    * \param min_pts_per_cluster minimum number of points that a cluster may contain (default: 1)
    * \param max_pts_per_cluster maximum number of points that a cluster may contain (default: max int)
    * \ingroup segmentation
    */
  template <typename PointT, typename Normal> 
  void extractEuclideanClusters (
      const PointCloud<PointT> &cloud, const PointCloud<Normal> &normals,
      const Indices &indices, const typename search::Search<PointT>::Ptr &tree,
      float tolerance, std::vector<PointIndices> &clusters, double max_angle,
      unsigned int min_pts_per_cluster = 1,
      unsigned int max_pts_per_cluster = (std::numeric_limits<int>::max) ())
  {
    if (cloud.points.size () != normals.points.size ())
    {
      PCL_ERROR ("[pcl::extractEuclideanClusters] Number of points in the input point cloud (%lu) different than normals (%lu)!\n", cloud.points.size (), normals.points.size ());
      return;
    }

    if (indices.empty())
      return;

    max_angle = std::min(std::abs(max_angle), M_PI);
    auto cos_max_angle = std::cos(max_angle);
    auto normal_deviation_filter = [&](const PointCloud<PointT> &cloud, index_t i, const Indices& nn_indices, index_t j) -> bool {
      double dot_p =
          normals[indices[i]].getNormalVector3fMap().dot(normals[indices[nn_indices[j]]].getNormalVector3fMap());
      return std::abs(dot_p) < cos_max_angle;
    };

    pcl::extractEuclideanClusters(cloud, indices, normal_deviation_filter, tree, tolerance, clusters, min_pts_per_cluster, max_pts_per_cluster);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief @b EuclideanClusterExtraction represents a segmentation class for cluster extraction in an Euclidean sense.
    * \author Radu Bogdan Rusu
    * \ingroup segmentation
    */
  template <typename PointT>
  class EuclideanClusterExtraction: public PCLBase<PointT>
  {
    using BasePCLBase = PCLBase<PointT>;

    public:
      using PointCloud = pcl::PointCloud<PointT>;
      using PointCloudPtr = typename PointCloud::Ptr;
      using PointCloudConstPtr = typename PointCloud::ConstPtr;

      using KdTree = pcl::search::Search<PointT>;
      using KdTreePtr = typename KdTree::Ptr;

      using PointIndicesPtr = PointIndices::Ptr;
      using PointIndicesConstPtr = PointIndices::ConstPtr;

      //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      /** \brief Empty constructor. */
      EuclideanClusterExtraction () : tree_ (), 
                                      cluster_tolerance_ (0),
                                      min_pts_per_cluster_ (1), 
                                      max_pts_per_cluster_ (std::numeric_limits<int>::max ())
      {};

      /** \brief Provide a pointer to the search object.
        * \param[in] tree a pointer to the spatial search object.
        */
      inline void 
      setSearchMethod (const KdTreePtr &tree) 
      { 
        tree_ = tree; 
      }

      /** \brief Get a pointer to the search method used. 
       *  @todo fix this for a generic search tree
       */
      inline KdTreePtr 
      getSearchMethod () const 
      { 
        return (tree_); 
      }

      /** \brief Set the spatial cluster tolerance as a measure in the L2 Euclidean space
        * \param[in] tolerance the spatial cluster tolerance as a measure in the L2 Euclidean space
        */
      inline void 
      setClusterTolerance (double tolerance) 
      { 
        cluster_tolerance_ = tolerance; 
      }

      /** \brief Get the spatial cluster tolerance as a measure in the L2 Euclidean space. */
      inline double 
      getClusterTolerance () const 
      { 
        return (cluster_tolerance_); 
      }

      /** \brief Set the minimum number of points that a cluster needs to contain in order to be considered valid.
        * \param[in] min_cluster_size the minimum cluster size
        */
      inline void 
      setMinClusterSize (int min_cluster_size) 
      { 
        min_pts_per_cluster_ = min_cluster_size; 
      }

      /** \brief Get the minimum number of points that a cluster needs to contain in order to be considered valid. */
      inline int 
      getMinClusterSize () const 
      { 
        return (min_pts_per_cluster_); 
      }

      /** \brief Set the maximum number of points that a cluster needs to contain in order to be considered valid.
        * \param[in] max_cluster_size the maximum cluster size
        */
      inline void 
      setMaxClusterSize (int max_cluster_size) 
      { 
        max_pts_per_cluster_ = max_cluster_size; 
      }

      /** \brief Get the maximum number of points that a cluster needs to contain in order to be considered valid. */
      inline int 
      getMaxClusterSize () const 
      { 
        return (max_pts_per_cluster_); 
      }

      /** \brief Cluster extraction in a PointCloud given by <setInputCloud (), setIndices ()>
        * \param[out] clusters the resultant point clusters
        */
      void 
      extract (std::vector<PointIndices> &clusters);

    protected:
      // Members derived from the base class
      using BasePCLBase::input_;
      using BasePCLBase::indices_;
      using BasePCLBase::initCompute;
      using BasePCLBase::deinitCompute;

      /** \brief A pointer to the spatial search object. */
      KdTreePtr tree_;

      /** \brief The spatial cluster tolerance as a measure in the L2 Euclidean space. */
      double cluster_tolerance_;

      /** \brief The minimum number of points that a cluster needs to contain in order to be considered valid (default = 1). */
      int min_pts_per_cluster_;

      /** \brief The maximum number of points that a cluster needs to contain in order to be considered valid (default = MAXINT). */
      int max_pts_per_cluster_;

      /** \brief Class getName method. */
      virtual std::string getClassName () const { return ("EuclideanClusterExtraction"); }

  };

  /** \brief Sort clusters method (for std::sort). 
    * \ingroup segmentation
    */
  inline bool 
  comparePointClusters (const pcl::PointIndices &a, const pcl::PointIndices &b)
  {
    return (a.indices.size () < b.indices.size ());
  }
}

#ifdef PCL_NO_PRECOMPILE
#include <pcl/segmentation/impl/extract_clusters.hpp>
#endif
